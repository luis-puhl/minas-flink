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
 * - Data points are represented as two Double values separated by a blank character.
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
 * [[KMeansData]]
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
    
    val points: DataSet[Point] =
      if (params.has("points"))
        env.readCsvFile[Point](
          params.get("points"),
          fieldDelimiter = " ",
          includedFields = Array(0, 1))
      else {
        println("Executing K-Means example with default points data set.")
        println("Use --points to specify file input.")
        // env.fromCollection(KMeansData.POINTS map {
        //   case Array(x, y) => new Point(x.asInstanceOf[Double], y.asInstanceOf[Double])
        // })
        KMeansData.getPointDataSet(params, env)
      }
    val centroids: DataSet[Centroid] =
      if (params.has("centroids"))
        env.readCsvFile[Centroid](
          params.get("centroids"),
          fieldDelimiter = " ",
          includedFields = Array(0, 1, 2))
      else {
        println("Executing K-Means example with default centroid data set.")
        println("Use --centroids to specify file input.")
        // env.fromCollection(KMeansData.CENTROIDS map {
        //   case Array(id, x, y) =>
        //     new Centroid(id.asInstanceOf[Int], x.asInstanceOf[Double], y.asInstanceOf[Double])
        // })
        KMeansData.getCentroidDataSet(params, env)
      }
    val iterations = params.getInt("iterations", 10)

    val finalCentroids = centroids.iterate(iterations) { currentCentroids =>
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

}

