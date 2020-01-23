package br.ufscar.dc.ppgcc.gsdr.minas

class Og {

  import NoveltyDetection.SeparateFiles
  import java.io.IOException
  import java.util

  import java.io.BufferedReader
  import java.io.FileReader
  import java.io.FileWriter
  import java.io.IOException
  import java.util

  def SeparateFileOff(filenameOff: String): Map[String, Vector[(Int, String, List[Double])]] = {
    val bufferedSource = io.Source.fromFile(filenameOff)
    val stream: Vector[(Int, String, List[Double])] = bufferedSource.getLines
      .zipWithIndex
      .map {
        case (line: String, id: Int) =>
          val attributes = line.split(",")
          val label = attributes(attributes.length - 1)
          val values = attributes.filter(i => i != label).map(i => i.toDouble).toList
          (id, label, values)
      }.toVector
    stream.groupBy[String](i => i._2)
  }

  @throws[IOException]
  def offline: util.ArrayList[String] = { // creating one training file per class
    val classesOffline = SeparateFileOff(filenameOffline)
    System.out.print("\n Classes for the training phase (offline): ")
    System.out.println("" + classesOffline.size)
    for (cl <- classesOffline.) {
      System.out.println("Class " + cl)
      val vectorResClustering = new Array[Int](1)
      // if the algorithm for clustering is Clustream
      if (algClusteringOff.equals("clustream")) {
        clusterSet = createModelClustream(filenameOffline, cl, vectorResClustering)
        // if the algorithm for clustering is kmeans
      }
      else if (algClusteringOff.equalsIgnoreCase("kmeans")) clusterSet = createModelKMeans(filenameOffline + cl, numMicro, cl, null, vectorResClustering)
      model.addAll(clusterSet)
    }
    // recording the number of classes of the training phase
    for (g <- 0 until classesOffline.size) {
      fileOutClasses.write(classesOffline.get(g))
      fileOutClasses.write("\r\n")
    }
    fileOutClasses.close
    classesOffline
  }
}
