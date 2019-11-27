package examples

import org.apache.flink.api.scala._
import org.apache.flink.ml.common.ParameterMap
import org.apache.flink.ml.recommendation.ALS

object Ex30FlinkML extends App {
  val env = ExecutionEnvironment.getExecutionEnvironment

  val rawDataSet = env.readCsvFile[(Int, Int, Double)]("audioscrobbler-dataset/user_artist_data_small.txt", "\n", " ")
  val trainingSet = rawDataSet.first(40000)
  val testSet: DataSet[(Int, Int)] = rawDataSet.map(v => (v._1, v._2))
  val als = ALS()
      .setIterations(10)
      .setNumFactors(10)
      .setBlocks(100)
      .setTemporaryPath("tmp-als")
  val parameters = ParameterMap()
      .add(ALS.Lambda, 0.9)
      .add(ALS.Seed, 42L)

  als.fit(trainingSet, parameters)

  val predicted = als.predict(testSet)

  predicted.print()

  // execute program
  //  env.execute("Flink ML API with audioscroobbler dataset")
  /**
   * val r = new Random(100666001L)
   * def nextName(r: Random, isTitleCase: Boolean = true, words: Int = 2, lengthMu: Int = 3, lengthVar: Int = 5): String = {
   * (1 to words).map(i => {
   * val chars = ('a' to 'z')
   * val length = r.nextInt(lengthVar) + lengthMu
   * val iseq: IndexedSeq[String] = 1 to length map(i => chars(r.nextInt(chars.length)).toString)
   * val s = iseq.reduce(_ + _).toLowerCase
   * if (isTitleCase)
   *         s.substring(0, 1).toUpperCase + s.substring(1, s.length)
   * else
   * s
   * }).reduce(_ + " " +  _)
   * }
   * val names = List.fill(100)(nextName(r))
   * val products = List.fill(100)(nextName(r, false, 1))
   * def choose(list: List[String], r: Random): String = list(r.nextInt(list.length))
   * val trainingData = List.fill(1000)( (choose(names, r), choose(products, r), r.nextInt(10)) )
   * val testData = List.fill(1000)( (choose(names, r), choose(products, r) ) )
   */
}
