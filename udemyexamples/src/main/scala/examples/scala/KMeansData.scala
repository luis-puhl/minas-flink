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

object KMeansData {
  def getCentroidDataSet(params: ParameterTool, env: ExecutionEnvironment): DataSet[Centroid] =
    env.fromCollection(List(
      Centroid(1, -31.85, -44.77), Centroid(2, 35.16, 17.46),
      Centroid(3, -5.16, 21.93), Centroid(4, -24.06, 6.81)
    ))

  def getPointDataSet(params: ParameterTool, env: ExecutionEnvironment): DataSet[Point] = {
    val points = List(
      (-14.22, -48.01), (-22.78, 37.10), (56.18, -42.99), (35.04, 50.29),
      (-9.53, -46.26), (-34.35, 48.25), (55.82, -57.49), (21.03, 54.64),
      (-13.63, -42.26), (-36.57, 32.63), (50.65, -52.40), (24.48, 34.04),
      (-2.69, -36.02), (-38.80, 36.58), (24.00, -53.74), (32.41, 24.96),
      (-4.32, -56.92), (-22.68, 29.42), (59.02, -39.56), (24.47, 45.07),
      (5.23, -41.20), (-23.00, 38.15), (44.55, -51.50), (14.62, 59.06),
      (7.41, -56.05), (-26.63, 28.97), (47.37, -44.72), (29.07, 51.06),
      (0.59, -31.89), (-39.09, 20.78), (42.97, -48.98), (34.36, 49.08),
      (-21.91, -49.01), (-46.68, 46.04), (48.52, -43.67), (30.05, 49.25),
      (4.03, -43.56), (-37.85, 41.72), (38.24, -48.32), (20.83, 57.85)
    )
    val pointsCollection: List[Point] = points.map(x => Point(x._1, x._2))
    env.fromCollection(pointsCollection)
  }
}