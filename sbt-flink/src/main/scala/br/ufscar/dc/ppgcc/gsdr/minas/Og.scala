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

// import java.io.IOException
// import java.{lang, util}
// import java.io.BufferedReader
// import java.io.FileReader

// import moa.cluster.{CFCluster, Cluster, Clustering, SphereCluster}
// import moa.clusterers.AbstractClusterer
// import moa.core.Measurement
// import moa.options.IntOption
// import weka.core.DenseInstance
// import weka.core.Instance

// import scala.collection.mutable


//class Og(val offlineFileName: String, val algClusteringOff: String, var model: List[Og#Cluster]) {
//  case class Cluster(meandist: Double, center: List[Double], size: Double, radius: Double, lblClasse: String, cat: String, time: Int)
//
//  class ClustreamKernelMOAModified(instance: Instance, dimensions: Int, timestamp: Long, label: String) extends CFCluster(instance, dimensions) {
//    var LST = timestamp
//    var SST = timestamp * timestamp
//
//    def insert(instance: Instance, timestamp: Long): Unit = {
//      N += 1
//      LST += timestamp
//      SST += timestamp * timestamp
//      (0 until instance.numValues()).foreach(i => {
//        LS(i) += instance.value(i)
//        SS(i) += instance.value(i) * instance.value(i)
//      })
//    }
//  }
//
//  class ClustreamMOAModified(bufferSize: Int) extends AbstractClusterer {
//    var timeWindowOption: IntOption = new IntOption("timeWindow", 't', "Rang of the window.", 1000)
//    var maxNumKernelsOption: IntOption = new IntOption("maxNumKernels", 'k', "Maximum number of micro kernels to use.", 100)
//    //
//    var initialized: Boolean = false
//    var timeWindow: Int = 0
//    var timestamp = -1
//    //
//    var buffer: mutable.Seq[ClustreamKernelMOAModified] = mutable.Seq[ClustreamKernelMOAModified]()
//    var elemMicroClusters: List[List[Int]] = List.fill(bufferSize)(List[Int]());
//    var kernels: mutable.Seq[ClustreamKernelMOAModified] = mutable.Seq[Og#ClustreamKernelMOAModified]()
//
//    override def resetLearningImpl(): Unit = {
//      this.initialized = false
//      this.timeWindow = timeWindowOption.getValue
//      this.buffer = mutable.Seq[ClustreamKernelMOAModified]()
//    }
//
//    def distance(pointA: Array[Double], pointB: Array[Double]): Double =
//      Math.sqrt(pointA.zip(pointB).map {
//        case (a, b) => (a - b) * (a - b)
//      }.sum)
//
//    override def trainOnInstanceImpl(instance: Instance): Unit = {
//      /*
//      val dim = instance.numValues
//      timestamp += 1
//      // 0. Initialize
//      if (!initialized) {
//        if (buffer.size < bufferSize) {
//          buffer :+ new Og#ClustreamKernelMOAModified(instance, dim, timestamp)
//          return
//        }
//        val k = kernels.length
//        assert(k < bufferSize)
//        val centers = new Array[Og#ClustreamKernelMOAModified](k)
//        for (i <- 0 until k) {
//          centers(i) = buffer.get(i) // TODO: make random!
//
//        }
//        val kmeans_clustering = kMeans(k, centers, buffer, elemMicroClusters)
//        for (i <- 0 until kmeans_clustering.size) {
//          kernels(i) = new Og#ClustreamKernelMOAModified(new DenseInstance(1.0, centers(i).getCenter), dim, timestamp)
//        }
//        buffer.clear
//        initialized = true
//        //return;
//      }
//       */
//      // 1. Determine closest kernel
//      val (kernelProximo: Int, closestKernel: ClustreamKernelMOAModified, minDistance: Double) = kernels.zipWithIndex
//        .map(k => (k._2, k._1, distance(instance.toDoubleArray, k._1.getCenter)))
//        .minBy(_._3)
//      // 2. Check whether instance fits into closestKernel// 2. Check whether instance fits into closestKernel
//
//      val radius: Double = if (closestKernel.getWeight eq 1) {
//        // Special case: estimate radius by determining the distance to the
//        // next closest cluster
//        val center = closestKernel.getCenter
//        kernels.filterNot(k => k == closestKernel).map(k => distance(k.getCenter, center)).min
//      }
//      else closestKernel.getRadius
//
//      if (minDistance < radius) {
//        // Date fits, put into kernel and be happy
//        closestKernel.insert(instance, timestamp)
//        elemMicroClusters(kernelProximo).add(timestamp.asInstanceOf[Int])
//        return
//      }
//    }
//
//    override def getModelMeasurementsImpl: Array[Measurement] = ???
//
//    override def getModelDescription(stringBuilder: lang.StringBuilder, i: Int): Unit = ???
//
//    override def isRandomizable: Boolean = ???
//
//    override def getVotesForInstance(instance: Instance): Array[Double] = ???
//
//    override def getClusteringResult: Clustering = ???
//  }
//
//  class ClustreamOffline {
//    private var microClusters = null
//    var resultingClusters: Nothing = null //can be micro or macro clusters
//
//    private var flagMicro = false
//    private var numResultingClusters = 0
//    private var clusterSize = null
//    private var classLabel = null //label of the class to which will be modeled a set of clusters to represent it
//
//    private var clusterExample = null //vector of size: number of examples. Each position contains the number of the cluster in which the example was classified
//
//    val elemMacroCluster = new util.ArrayList[util.ArrayList[Integer]]
//    private var meanDistance = null
//
//
//    @throws[IOException]
//    def CluStreamOff(classLabel: String, filepath: String, nMicro: Float, flagMicro: Boolean): util.ArrayList[Array[Double]] = {
//      this.classLabel = classLabel
//      this.flagMicro = flagMicro
//      microClusters = null
//      var fileIn = null
//      var line = null
//      var macroClusters = null
//      // ClustreamMOAModified algClustream = new ClustreamMOAModified();
//      val algClustream = new Nothing
//      algClustream.prepareForUse
//      algClustream.InicializaBufferSize(nMicro.toInt)
//      //reading the datafile and creating the set of clusters
//      var size = 0
//      fileIn = new BufferedReader(new FileReader(new Nothing(filepath + classLabel)))
//      var fileLine = null
//      while ( {
//        (line = fileIn.readLine) != null
//      }) {
//        val fileLineString = line.split(",")
//        size = fileLineString.length - 1
//        fileLine = new Array[Double](size)
//        for (i <- 0 until size) {
//          fileLine(i) = fileLineString(i).toDouble
//        }
//        val inst = new Nothing(1, fileLine)
//        algClustream.trainOnInstanceImpl(inst)
//      }
//      fileIn.close()
//      //get the resulting microclusters
//      microClusters = algClustream.getMicroClusteringResult
//      //vector containing for each micro-cluster their elements
//      var elemMicroCluster = new util.ArrayList[util.ArrayList[Integer]]
//      elemMicroCluster = algClustream.getMicroClusterElements
//      val centers = new util.ArrayList[Array[Double]]
//      //********************if the microclusters have to be macroclustered using the KMeans
//      if (flagMicro == false) {
//        //choose the best value for K and do the macroclustering using KMeans
//        macroClusters = KMeansMOAModified.OMRk(microClusters, 0, elemMacroCluster)
//        numResultingClusters = macroClusters.size
//        resultingClusters = macroClusters
//        //vector containing for each macro-clusters the elements in this cluster
//        var finalMacroClustering = null
//        finalMacroClustering = new Array[util.ArrayList[_]](numResultingClusters).asInstanceOf[Array[util.ArrayList[Integer]]]
//        for (i <- 0 until finalMacroClustering.length) {
//          finalMacroClustering(i) = new util.ArrayList[Integer]
//        }
//        //keep a vector of size: numberofMacroClusters
//        //each position keeps a vector with all elements that belongs to this clusters
//        for (i <- 0 until elemMacroCluster.size) {
//          for (j <- 0 until elemMacroCluster.get(i).size) {
//            val pos = elemMacroCluster.get(i).get(j)
//            for (k <- 0 until elemMicroCluster.get(pos).size) {
//              finalMacroClustering(i).add(elemMicroCluster.get(pos).get(k).asInstanceOf[Int])
//            }
//          }
//        }
//        clusterExample = getFinalMacroClustering(finalMacroClustering)
//        //number of elements in each macro-clusters
//        clusterSize = new Array[Double](macroClusters.size.asInstanceOf[Int])
//        for (k <- 0 until numResultingClusters) {
//          centers.add(macroClusters.getClustering.get(k).getCenter)
//          clusterSize(k) = elemMacroCluster.get(k).size
//        }
//      }
//      else {
//        //else: using the micro-clusters without macro-clustering
//        resultingClusters = microClusters
//        var nroelem = 0
//        for (i <- 0 until elemMicroCluster.size) {
//          nroelem += elemMicroCluster.get(i).size
//        }
//        clusterExample = new Array[Int](nroelem)
//        var value = 0
//        //to keep a vector of size: number of elements. Each position contains the number of the cluster of the corresponding element
//        var i = 0
//        while ( {
//          i < elemMicroCluster.size
//        }) {
//          //if the micro-cluster has less than 3 elements, then discard it
//          if (microClusters.get(i).asInstanceOf[Nothing].getWeight < 3) value = -1
//          else value = i
//          for (j <- 0 until elemMicroCluster.get(i).size) {
//            clusterExample(elemMicroCluster.get(i).get(j)) = value
//          }
//          if (microClusters.get(i).asInstanceOf[Nothing].getWeight < 3) {
//            microClusters.remove(i)
//            elemMicroCluster.remove(i)
//            i -= 1
//          }
//
//          i += 1
//        }
//        numResultingClusters = microClusters.size.asInstanceOf[Int]
//        clusterSize = new Array[Double](numResultingClusters)
//        for (k <- 0 until numResultingClusters) {
//          centers.add(microClusters.getClustering.get(k).getCenter)
//          clusterSize(k) = elemMicroCluster.get(k).size
//        }
//      }
//      centers
//    }
//
//    def getClusterSize: Array[Double] = clusterSize
//
//    def getRadius: Array[Double] = {
//      val radius = new Array[Double](numResultingClusters)
//      meanDistance = new Array[Double](numResultingClusters)
//      if (flagMicro == false) {
//        //in this case it was used clustream + kmeans (macro-clustering in the micro-clusters)
//        var centeres = null
//        var maxDistance = .0
//        var elem = null
//        var sum = .0
//        var distance = .0
//        for (i <- 0 until numResultingClusters) {
//          // for each macro-clustering
//          centeres = resultingClusters.get(i).getCenter //obtaining the center of the macro-clusters
//
//          maxDistance = 0
//          for (j <- 0 until elemMacroCluster.get(i).size) {
//            //para cada elemento no macro-cluster
//            elem = microClusters.get(elemMacroCluster.get(i).get(j)).getCenter
//            sum = 0
//            for (l <- 0 until elem.length) {
//              //distancia do elem ao centro
//              sum = sum + Math.pow(centeres(l) - elem(l), 2)
//            }
//            distance = Math.sqrt(sum)
//            meanDistance(i) = meanDistance(i) + distance
//            if (distance > maxDistance) maxDistance = distance
//          }
//          radius(i) = maxDistance
//          //another option is to obtain the radius of the macro-clustering
//          radius(i) = resultingClusters.get(i).asInstanceOf[Nothing].getRadius
//        }
//      }
//      else {
//        //in this case only the micro-clusters will be used
//        for (i <- 0 until numResultingClusters) {
//          radius(i) = resultingClusters.get(i).asInstanceOf[Nothing].getRadius
//          meanDistance(i) = resultingClusters.get(i).asInstanceOf[Nothing].getRadius / 2
//        }
//      }
//      radius
//    }
//
//    def meanDistance: Array[Double] = {
//      val res = new Array[Double](numResultingClusters)
//      for (i <- 0 until numResultingClusters) {
//        if (flagMicro == false) res(i) = meanDistance(i) / clusterSize(i)
//        else res(i) = meanDistance(i)
//      }
//      res
//    }
//
//    def getClusterClass: Array[String] = {
//      val vetClusterClass = new Array[String](numResultingClusters)
//      for (i <- 0 until numResultingClusters) {
//        vetClusterClass(i) = classLabel
//      }
//      vetClusterClass
//    }
//
//    def getClusterExample: Array[Int] = clusterExample
//
//    def getFinalMacroClustering(finalMacroClustering: Array[util.ArrayList[Integer]]): Array[Int] = {
//      var result = null
//      var numberofElements = 0
//      for (i <- 0 until finalMacroClustering.length) {
//        numberofElements += finalMacroClustering(i).size
//      }
//      result = new Array[Int](numberofElements)
//      for (i <- 0 until finalMacroClustering.length) {
//        for (j <- 0 until finalMacroClustering(i).size) {
//          result(finalMacroClustering(i).get(j)) = i + 1
//        }
//      }
//      result
//    }
//  }
//
//
//  def separateFileOff: Map[String, Vector[(Int, String, List[Double])]] = {
//    val bufferedSource = io.Source.fromFile(offlineFileName)
//    val stream = bufferedSource.getLines
//      .zipWithIndex
//      .map {
//        case (line: String, id: Int) =>
//          val attributes = line.split(",")
//          val label = attributes(attributes.length - 1)
//          val values = attributes.filter(i => i != label).map(i => i.toDouble).toList
//          (id, label, values)
//      }.toVector
//    stream.groupBy[String](i => i._2)
//  }
//
//  @throws[NumberFormatException]
//  @throws[IOException]
//  def createModelClustream(filename: String, cl: String, vectorResClustering: Array[Int]): util.ArrayList[Nothing] = {
//    var centers = null
//    val clusterSet = new util.ArrayList[Nothing]
//    val jc = new Nothing
//    //executing the clustream offline
//    centers = jc.CluStreamOff(cl, filename, numMicro, flagMicroClusters)
//    //results
//    val size = jc.getClusterSize
//    val radius = jc.getRadius
//    val meanDistance = jc.meanDistance
//    val classLabel = jc.getClusterClass
//    if (currentState.compareTo("online") eq 0) {
//      val tmp = jc.getClusterExample
//
//      for (i <- 0 until tmp.length) {
//        vectorResClustering(i) = tmp(i)
//      }
//    }
//    // one class is represented by a set of Clusters
//    // creating a clusterModel for each cluster
//    for (w <- 0 until size.length) {
//      val clus = new Nothing(meanDistance(w), centers.get(w), size(w), radius(w), classLabel(w), "normal", timestamp)
//      clusterSet.add(clus)
//    }
//    clusterSet
//  }
//
//  @throws[IOException]
//  def offline: util.ArrayList[String] = {
//    val classesOffline = separateFileOff
//    print("\n Classes for the training phase (offline): ")
//    println("" + classesOffline.size)
//    for ((label, entries) <- classesOffline) {
//      println(s"Class $label")
//      val vectorResClustering = new Array[Int](1)
//      model ++ (algClusteringOff match {
//        case "clustream" => createModelClustream(entries, label, vectorResClustering)
//        case "kmeans" => createModelKMeans(filenameOffline + cl, numMicro, cl, null, vectorResClustering)
//      })
//    }
//    // recording the number of classes of the training phase
//    for (g <- 0 until classesOffline.size) {
//      fileOutClasses.write(classesOffline.get(g))
//      fileOutClasses.write("\r\n")
//    }
//    fileOutClasses.close
//    classesOffline
//  }
//}
