package examples.scala

import java.io.{BufferedWriter, File, FileWriter, IOException}
import java.text.DecimalFormat

import scala.collection.mutable.StringBuilder
import org.apache.flink.api.common.functions._
import org.apache.flink.api.java.functions.FunctionAnnotation.ForwardedFields
import org.apache.flink.api.java.utils.ParameterTool
import org.apache.flink.api.scala._
import org.apache.flink.configuration.Configuration

import scala.collection.JavaConverters._
import scala.util.{Random, Try}

object KMeansDataGenerator {
  val CENTERS_FILE: String = "centers"
  val POINTS_FILE: String = "points"
  val DEFAULT_SEED: Long = 4650285087650871364L
  val DEFAULT_VALUE_RANGE: Double = 100.0
  val RELATIVE_STDDEV: Double = 0.08
  val DIMENSIONALITY: Int = 2
  val FORMAT: DecimalFormat = new DecimalFormat("#0.00")
  val DELIMITER: Char = ' '

  /**
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
   * @throws IOException
   */
  def main(args: Array[String]) = {
    // check parameter count
    if (args.length < 2)
      throw new RuntimeException("KMeansDataGenerator -points <num> -k <num clusters> [-output <output-path>] [-stddev <relative stddev>] [-range <centroid range>] [-seed <seed>]")
    // parse parameters
    val params: ParameterTool = ParameterTool.fromArgs(args)
    // set up execution environment
    val env: ExecutionEnvironment = ExecutionEnvironment.getExecutionEnvironment

    val numDataPoints: Int = params.getInt("points")
    val k: Int = params.getInt("k")
    val outDir: String = params.get("output", System.getProperty("java.io.tmpdir"))
    val stddev: Double = params.getDouble("stddev", RELATIVE_STDDEV)
    val range: Double = params.getDouble("range", DEFAULT_VALUE_RANGE)
    val firstSeed: Long = params.getLong("seed", DEFAULT_SEED)

    val absoluteStdDev: Double = stddev * range
    val random: Random = new Random(firstSeed)

    // the means around which data points are distributed
    val means = uniformRandomCenters(random, k, DIMENSIONALITY, range);

    // write the points out
    val buffer: StringBuilder = new StringBuilder()
    (for {
      pointsOut <- Try { new BufferedWriter(new FileWriter(new File(outDir + "/" + POINTS_FILE))) }.toOption.toList
      point <- (0 to numDataPoints)
        .map(i => means(i % means.size)
          .map(ci => ci + (random.nextGaussian() * absoluteStdDev)))
      w <- writePoint(point, buffer, pointsOut)
    } yield pointsOut).andThen(p => p.close())

    // write the uniformly distributed centers to a file
    (for {
      centersOut: BufferedWriter <- Try { new BufferedWriter(new FileWriter(new File(outDir + "/" + CENTERS_FILE))) }.toOption.toList
      center <- (1 to k)
        .map(i => (i,
          (0 to DIMENSIONALITY).map(j => (random.nextDouble() * range) - (range / 2))
        )).toList
      w <- writeCenter(center._1, center._2, buffer, centersOut)
    } yield centersOut).andThen(p => p.close())

    println("Wrote " + numDataPoints + " data points to " + outDir + "/" + POINTS_FILE);
    println("Wrote " + k + " cluster centers to " + outDir + "/" + CENTERS_FILE);
  }

  def uniformRandomCenters(rnd: Random, num: Int, dimensionality: Int, range: Double): List[List[Double]] =
    List.fill(num)(List.fill(dimensionality)(() => (rnd.nextDouble() * range) - (range / 2)).map(_.apply()))

  def writePoint(coordinates: Seq[Double], buffer: StringBuilder, out: BufferedWriter): Seq[Double] = {
    buffer.setLength(0)
    coordinates.map(FORMAT.format).addString(buffer, DELIMITER.toString)
    out.write(buffer.toString())
    out.newLine()
    coordinates
  }

  def writeCenter(id: Long, coordinates: Seq[Double], buffer: StringBuilder, out: BufferedWriter): Seq[Double] = {
    buffer.setLength(0)
    // write id
    buffer.append(id)
    buffer.append(DELIMITER)

    coordinates.map(FORMAT.format).addString(buffer, DELIMITER.toString)
    out.write(buffer.toString())
    out.newLine()
    coordinates
  }
}
