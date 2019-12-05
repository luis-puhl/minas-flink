/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package examples.scala

import org.apache.flink.api.common.functions._
import org.apache.flink.api.java.functions.FunctionAnnotation.ForwardedFields
import org.apache.flink.api.java.utils.ParameterTool
import org.apache.flink.api.scala._
import org.apache.flink.configuration.Configuration

import scala.collection.JavaConverters._

/**
 * This example implements a basic K-Means clustering algorithm.
 *
 * K-Means is an iterative clustering algorithm and works as follows:
 * K-Means is given a set of data points to be clustered and an initial set of ''K'' cluster
 * centers.
 * In each iteration, the algorithm computes the distance of each data point to each cluster center.
 * Each point is assigned to the cluster center which is closest to it.
 * Subsequently, each cluster center is moved to the center (''mean'') of all points that have
 * been assigned to it.
 * The moved cluster centers are fed into the next iteration.
 * The algorithm terminates after a fixed number of iterations (as in this implementation)
 * or if cluster centers do not (significantly) move in an iteration.
 * This is the Wikipedia entry for the [[http://en.wikipedia
 * .org/wiki/K-means_clustering K-Means Clustering algorithm]].
 *
 * This implementation works on two-dimensional data points.
 * It computes an assignment of data points to cluster centers, i.e.,
 * each data point is annotated with the id of the final cluster (center) it belongs to.
 *
 * Input files are plain text files and must be formatted as follows:
 *
 * - Data points are represented as two double values separated by a blank character.
 * Data points are separated by newline characters.
 * For example `"1.2 2.3\n5.3 7.2\n"` gives two data points (x=1.2, y=2.3) and (x=5.3,
 * y=7.2).
 * - Cluster centers are represented by an integer id and a point value.
 * For example `"1 6.2 3.2\n2 2.9 5.7\n"` gives two centers (id=1, x=6.2,
 * y=3.2) and (id=2, x=2.9, y=5.7).
 *
 * Usage:
 * {{{
 *   KMeans --points <path> --centroids <path> --output <path> --iterations <n>
 * }}}
 * If no parameters are provided, the program is run with default data from
 * [[org.apache.flink.examples.java.clustering.util.KMeansData]]
 * and 10 iterations.
 *
 * This example shows how to use:
 *
 * - Bulk iterations
 * - Broadcast variables in bulk iterations
 * - Scala case classes
 */
object KMeans {

  def main(args: Array[String]) {

    // checking input parameters
    val params: ParameterTool = ParameterTool.fromArgs(args)

    // set up execution environment
    val env: ExecutionEnvironment = ExecutionEnvironment.getExecutionEnvironment

    // get input data:
    // read the points and centroids from the provided paths or fall back to default data
    val points: DataSet[Point] = getPointDataSet(params, env)
    val centroids: DataSet[Centroid] = getCentroidDataSet(params, env)

    val finalCentroids = centroids.iterate(params.getInt("iterations", 10)) { currentCentroids =>
      val newCentroids = points
        .map(new SelectNearestCenter).withBroadcastSet(currentCentroids, "centroids")
        .map { x => (x._1, x._2, 1L) }.withForwardedFields("_1; _2")
        .groupBy(0)
        .reduce { (p1, p2) => (p1._1, p1._2.add(p2._2), p1._3 + p2._3) }.withForwardedFields("_1")
        .map { x => new Centroid(x._1, x._2.div(x._3)) }.withForwardedFields("_1->id")
      newCentroids
    }

    val clusteredPoints: DataSet[(Int, Point)] =
      points.map(new SelectNearestCenter).withBroadcastSet(finalCentroids, "centroids")

    if (params.has("output")) {
      clusteredPoints.writeAsCsv(params.get("output"), "\n", " ")
      env.execute("Scala KMeans Example")
    } else {
      println("Printing result to stdout. Use --output to specify output path.")
      clusteredPoints.print()
    }

  }

  // *************************************************************************
  //     DATA TYPES
  // *************************************************************************

  /**
    * Common trait for operations supported by both points and centroids
    * Note: case class inheritance is not allowed in Scala
    */
  trait Coordinate extends Serializable {

    var x: Double
    var y: Double

    def add(other: Coordinate): this.type = {
      x += other.x
      y += other.y
      this
    }

    def div(other: Long): this.type = {
      x /= other
      y /= other
      this
    }

    def euclideanDistance(other: Coordinate): Double =
      Math.sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y))

    def clear(): Unit = {
      x = 0
      y = 0
    }

    override def toString: String =
      s"$x $y"

  }

  /**
    * A simple two-dimensional point.
    */
  case class Point(var x: Double = 0, var y: Double = 0) extends Coordinate

  /**
    * A simple two-dimensional centroid, basically a point with an ID.
    */
  case class Centroid(var id: Int = 0, var x: Double = 0, var y: Double = 0) extends Coordinate {

    def this(id: Int, p: Point) {
      this(id, p.x, p.y)
    }

    override def toString: String =
      s"$id ${super.toString}"

  }

  /** Determines the closest cluster center for a data point. */
  @ForwardedFields(Array("*->_2"))
  final class SelectNearestCenter extends RichMapFunction[Point, (Int, Point)] {
    private var centroids: Traversable[Centroid] = null

    /** Reads the centroid values from a broadcast variable into a collection. */
    override def open(parameters: Configuration) {
      centroids = getRuntimeContext.getBroadcastVariable[Centroid]("centroids").asScala
    }

    def map(p: Point): (Int, Point) = {
      var minDistance: Double = Double.MaxValue
      var closestCentroidId: Int = -1
      for (centroid <- centroids) {
        val distance = p.euclideanDistance(centroid)
        if (distance < minDistance) {
          minDistance = distance
          closestCentroidId = centroid.id
        }
      }
      (closestCentroidId, p)
    }

  }
  object KMeansData {
    def getCentroidDataSet(params: ParameterTool, env: ExecutionEnvironment): DataSet[Centroid] =
      env.fromCollection(List(
        Centroid(1, -31.85, -44.77),  Centroid(2, 35.16, 17.46),
        Centroid(3, -5.16, 21.93),    Centroid(4, -24.06, 6.81),
      ))

    def getPointDataSet(params: ParameterTool, env: ExecutionEnvironment): DataSet[Point] = {
      val points = List(
        (-14.22, -48.01), (-22.78, 37.10),    (56.18, -42.99),    (35.04, 50.29),
        (-9.53, -46.26),  (-34.35, 48.25),    (55.82, -57.49),    (21.03, 54.64),
        (-13.63, -42.26), (-36.57, 32.63),    (50.65, -52.40),    (24.48, 34.04),
        (-2.69, -36.02),  (-38.80, 36.58),    (24.00, -53.74),    (32.41, 24.96),
        (-4.32, -56.92),  (-22.68, 29.42),    (59.02, -39.56),    (24.47, 45.07),
        (5.23, -41.20),   (-23.00, 38.15),    (44.55, -51.50),    (14.62, 59.06),
        (7.41, -56.05),   (-26.63, 28.97),    (47.37, -44.72),    (29.07, 51.06),
        (0.59, -31.89),   (-39.09, 20.78),    (42.97, -48.98),    (34.36, 49.08),
        (-21.91, -49.01), (-46.68, 46.04),    (48.52, -43.67),    (30.05, 49.25),
        (4.03, -43.56),   (-37.85, 41.72),    (38.24, -48.32),    (20.83, 57.85)
      )
      val pointsCollection: List[Point] = points.map[Point](x => Point(x._1, x._2))
      env.fromCollection(pointsCollection)
    }
  }
}

/*
object KMeansDataGenerator {
    val CENTERS_FILE: String = "centers"
    val POINTS_FILE: String = "points"
    val DEFAULT_SEED: long = 4650285087650871364L
    val DEFAULT_VALUE_RANGE: double = 100.0
    val RELATIVE_STDDEV: double = 0.08
    val DIMENSIONALITY: int = 2
    val FORMAT: DecimalFormat = new DecimalFormat("#0.00")
    val DELIMITER: char = ' '

    // /**
     * Main method to generate data for the {@link KMeans} example program.
     *
     * <p>The generator creates to files:
     * <ul>
     * <li><code>&lt; output-path &gt;/points</code> for the data points
     * <li><code>&lt; output-path &gt;/centers</code> for the cluster centers
     * </ul>
     *
     * @param args
     * <ol>
     * <li>Int: Number of data points
     * <li>Int: Number of cluster centers
     * <li><b>Optional</b> String: Output path, default value is {tmp.dir}
     * <li><b>Optional</b> Double: Standard deviation of data points
     * <li><b>Optional</b> Double: Value range of cluster centers
     * <li><b>Optional</b> Long: Random seed
     * </ol>
     *
     * @throws IOException
     */
    public static void main(String[] args) throws IOException {

        // check parameter count
        if (args.length < 2) {
            System.out.println("KMeansDataGenerator -points <num> -k <num clusters> [-output <output-path>] [-stddev <relative stddev>] [-range <centroid range>] [-seed <seed>]");
            System.exit(1);
        }

        // parse parameters

        final ParameterTool params = ParameterTool.fromArgs(args);
        final int numDataPoints = params.getInt("points");
        final int k = params.getInt("k");
        final String outDir = params.get("output", System.getProperty("java.io.tmpdir"));
        final double stddev = params.getDouble("stddev", RELATIVE_STDDEV);
        final double range = params.getDouble("range", DEFAULT_VALUE_RANGE);
        final long firstSeed = params.getLong("seed", DEFAULT_SEED);

        final double absoluteStdDev = stddev * range;
        final Random random = new Random(firstSeed);

        // the means around which data points are distributed
        final double[][] means = uniformRandomCenters(random, k, DIMENSIONALITY, range);

        // write the points out
        BufferedWriter pointsOut = null;
        try {
            pointsOut = new BufferedWriter(new FileWriter(new File(outDir + "/" + POINTS_FILE)));
            StringBuilder buffer = new StringBuilder();

            double[] point = new double[DIMENSIONALITY];
            int nextCentroid = 0;

            for (int i = 1; i <= numDataPoints; i++) {
                // generate a point for the current centroid
                double[] centroid = means[nextCentroid];
                for (int d = 0; d < DIMENSIONALITY; d++) {
                    point[d] = (random.nextGaussian() * absoluteStdDev) + centroid[d];
                }
                writePoint(point, buffer, pointsOut);
                nextCentroid = (nextCentroid + 1) % k;
            }
        }
        finally {
            if (pointsOut != null) {
                pointsOut.close();
            }
        }

        // write the uniformly distributed centers to a file
        BufferedWriter centersOut = null;
        try {
            centersOut = new BufferedWriter(new FileWriter(new File(outDir + "/" + CENTERS_FILE)));
            StringBuilder buffer = new StringBuilder();

            double[][] centers = uniformRandomCenters(random, k, DIMENSIONALITY, range);

            for (int i = 0; i < k; i++) {
                writeCenter(i + 1, centers[i], buffer, centersOut);
            }
        }
        finally {
            if (centersOut != null) {
                centersOut.close();
            }
        }

        System.out.println("Wrote " + numDataPoints + " data points to " + outDir + "/" + POINTS_FILE);
        System.out.println("Wrote " + k + " cluster centers to " + outDir + "/" + CENTERS_FILE);
    }

    private static double[][] uniformRandomCenters(Random rnd, int num, int dimensionality, double range) {
        final double halfRange = range / 2;
        final double[][] points = new double[num][dimensionality];

        for (int i = 0; i < num; i++) {
            for (int dim = 0; dim < dimensionality; dim++) {
                points[i][dim] = (rnd.nextDouble() * range) - halfRange;
            }
        }
        return points;
    }

    private static void writePoint(double[] coordinates, StringBuilder buffer, BufferedWriter out) throws IOException {
        buffer.setLength(0);

        // write coordinates
        for (int j = 0; j < coordinates.length; j++) {
            buffer.append(FORMAT.format(coordinates[j]));
            if (j < coordinates.length - 1) {
                buffer.append(DELIMITER);
            }
        }

        out.write(buffer.toString());
        out.newLine();
    }

    private static void writeCenter(long id, double[] coordinates, StringBuilder buffer, BufferedWriter out) throws IOException {
        buffer.setLength(0);

        // write id
        buffer.append(id);
        buffer.append(DELIMITER);

        // write coordinates
        for (int j = 0; j < coordinates.length; j++) {
            buffer.append(FORMAT.format(coordinates[j]));
            if (j < coordinates.length - 1) {
                buffer.append(DELIMITER);
            }
        }

        out.write(buffer.toString());
        out.newLine();
    }
}
*/
