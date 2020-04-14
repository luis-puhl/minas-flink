package br.ufscar.dc.ppgcc.gsdr.mfog

import java.io.{BufferedWriter, File, FileWriter, PrintStream}
import java.net.{InetAddress, Socket}
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

import grizzled.slf4j.Logger
import org.apache.flink.api.scala._
import org.apache.flink.api.common.functions.{MapFunction, ReduceFunction, RichMapFunction}
import org.apache.flink.configuration.Configuration
import org.apache.flink.streaming.api.datastream.BroadcastStream
import org.apache.flink.streaming.api.scala.DataStream
import org.apache.flink.streaming.api.scala.StreamExecutionEnvironment
import org.apache.flink.streaming.api.functions.co.{BroadcastProcessFunction, CoProcessFunction}
import org.apache.flink.util.Collector
import org.json.JSONObject

import scala.collection.{immutable, mutable}
import scala.io.BufferedSource

object Classifier {
  val LOG: Logger = Logger(getClass)
  def main(args: Array[String]): Unit = {
    val jobName = this.getClass.getName
    val dateString = LocalDateTime.now.format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-")
    LOG.info(s"jobName = $jobName")
    val outDir = s"./out/$jobName/$dateString/"
    val dir = new File(outDir)
    if (!dir.exists) {
      if (!dir.mkdirs) throw new RuntimeException(s"Output directory '$outDir'could not be created.")
    }

    // baseline(outDir, jobName)
    // 8.to(1, -1).foreach(par => run(outDir, jobName, par))
    val (model, data) = setUpWithFullStatic()
    8.to(1, -1).foreach(par => runWithStaticModel(outDir, jobName, par, model))
    // (8 to 1).foreach(par => runWithFullStatic(outDir, jobName, par, model, data))
    // plainScala(outDir, model, data)

  }

  class R(val p: Point, val c: Cluster, val d: Double, val l: String, val t: Long) {
    override def toString: String = s"$d,$l,$t,$p,$c"
  }

  /**
   * 14:48:49 INFO  mfog.Classifier$ jobName = br.ufscar.dc.ppgcc.gsdr.mfog.Classifier$
   * 14:50:21 INFO  mfog.Classifier$ setup in 92.634s
   * 14:51:12 INFO  mfog.Classifier$ plainScala in 50.645s
   * 14:51:12 INFO  mfog.Classifier$ plainScala average latency = 70.978 seconds
   *
   * @param outDir
   * @param model
   * @param data
   */
  def plainScala(outDir: String, model: immutable.Seq[Cluster], data: immutable.Seq[Point]): Unit = {
    val start = System.currentTimeMillis()
    val fp = new java.io.PrintWriter(new File(outDir + "plainScala"))
    val outStream = data.par.map(x => {
      val (d, c) = model.map(c => (x.euclideanDistance(c.center), c)).minBy(_._1)
      val l = if (d <= c.variance) c.label else "unk"
      new R(x, c, d, l, System.currentTimeMillis() - x.time)
    })
    outStream.foreach(out => fp.println(out.toString))
    fp.close()
    val avg = (outStream.map(_.t).sum / outStream.size) * 10e-4
    LOG.info(s"plainScala in ${(System.currentTimeMillis() - start) * 10e-4}s")
    LOG.info(s"plainScala average latency = $avg seconds")
  }

  /**
   * 14:50:21 INFO  mfog.Classifier$ setup in 92.634s
   * @return
   */
  def setUpWithFullStatic():  (immutable.Seq[Cluster], immutable.Seq[Point]) = {
    val start = System.currentTimeMillis()
    val model: immutable.Seq[Cluster] = {
      val socket = new Socket(InetAddress.getByName("localhost"), 9997)
      val values = new BufferedSource(socket.getInputStream).getLines().toVector.map(c => Cluster.fromJson(c))
      socket.close()
      values
    }
    val data: immutable.Seq[Point] = {
      val socket = new Socket(InetAddress.getByName("localhost"), 9996)
      val values = new BufferedSource(socket.getInputStream).getLines().toVector.map(c => Point.fromJson(c))
      socket.close()
      values
    }
    LOG.info(s"setup in ${(System.currentTimeMillis() - start) * 10e-4}s")
    (model, data)
  }

  /**
   * "C:\Program Files\Java\jdk1.8.0_201\bin\java.exe" "-javaagent:C:\Program Files\JetBrains\IntelliJ IDEA Community Edition 2019.3\lib\idea_rt.jar=55521:C:\Program Files\JetBrains\IntelliJ IDEA Community Edition 2019.3\bin" -Dfile.encoding=UTF-8 -classpath "C:\Program Files\Java\jdk1.8.0_201\jre\lib\charsets.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\deploy.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\access-bridge-64.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\cldrdata.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\dnsns.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\jaccess.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\jfxrt.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\localedata.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\nashorn.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\sunec.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\sunjce_provider.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\sunmscapi.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\sunpkcs11.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\ext\zipfs.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\javaws.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\jce.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\jfr.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\jfxswt.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\jsse.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\management-agent.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\plugin.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\resources.jar;C:\Program Files\Java\jdk1.8.0_201\jre\lib\rt.jar;C:\dev\minas-flink\minas\target\scala-2.11\classes;C:\Users\Luís Puhl\.ivy2\cache\com.esotericsoftware.kryo\kryo\bundles\kryo-2.24.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\force-shading\jars\force-shading-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-streaming-scala_2.11\jars\flink-streaming-scala_2.11-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-streaming-java_2.11\jars\flink-streaming-java_2.11-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-shaded-jackson\jars\flink-shaded-jackson-2.10.1-9.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-scala_2.11\jars\flink-scala_2.11-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-runtime_2.11\jars\flink-runtime_2.11-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-queryable-state-client-java\jars\flink-queryable-state-client-java-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-optimizer_2.11\jars\flink-optimizer_2.11-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-metrics-core\jars\flink-metrics-core-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-java\jars\flink-java-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-hadoop-fs\jars\flink-hadoop-fs-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-core\jars\flink-core-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-connector-kafka_2.11\jars\flink-connector-kafka_2.11-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-connector-kafka-base_2.11\jars\flink-connector-kafka-base_2.11-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-clients_2.11\jars\flink-clients_2.11-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-annotations\jars\flink-annotations-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.xerial.snappy\snappy-java\bundles\snappy-java-1.1.7.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.lz4\lz4-java\jars\lz4-java-1.5.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.kafka\kafka-clients\jars\kafka-clients-2.2.0.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.luben\zstd-jni\jars\zstd-jni-1.3.8-1.jar;C:\Users\Luís Puhl\.ivy2\cache\org.scala-lang\scala-swing\jars\scala-swing-2.10.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.markdownj\markdownj-core\jars\markdownj-core-0.4.jar;C:\Users\Luís Puhl\.ivy2\cache\org.kramerlab\bmad\jars\bmad-2.4.jar;C:\Users\Luís Puhl\.ivy2\cache\org.kramerlab\autoencoder\jars\autoencoder-0.1.jar;C:\Users\Luís Puhl\.ivy2\cache\org.jfree\jfreechart\jars\jfreechart-1.0.19.jar;C:\Users\Luís Puhl\.ivy2\cache\org.jfree\jcommon\jars\jcommon-1.0.23.jar;C:\Users\Luís Puhl\.ivy2\cache\org.codehaus.plexus\plexus-utils\jars\plexus-utils-3.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.codehaus.plexus\plexus-container-default\jars\plexus-container-default-1.0-alpha-9-stable-1.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven.wagon\wagon-provider-api\jars\wagon-provider-api-1.0-beta-2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven.reporting\maven-reporting-api\jars\maven-reporting-api-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven.doxia\doxia-sink-api\jars\doxia-sink-api-1.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven\maven-settings\jars\maven-settings-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven\maven-repository-metadata\jars\maven-repository-metadata-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven\maven-project\jars\maven-project-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven\maven-profile\jars\maven-profile-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven\maven-plugin-registry\jars\maven-plugin-registry-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven\maven-plugin-api\jars\maven-plugin-api-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven\maven-model\jars\maven-model-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven\maven-artifact-manager\jars\maven-artifact-manager-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.maven\maven-artifact\jars\maven-artifact-2.0.9.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.commons\commons-text\jars\commons-text-1.1.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.commons\commons-math3\jars\commons-math3-3.6.1.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.commons\commons-lang3\jars\commons-lang3-3.7.jar;C:\Users\Luís Puhl\.ivy2\cache\nz.ac.waikato.cms.weka.thirdparty\java-cup-11b-runtime\jars\java-cup-11b-runtime-2015.03.26.jar;C:\Users\Luís Puhl\.ivy2\cache\nz.ac.waikato.cms.weka.thirdparty\java-cup-11b\jars\java-cup-11b-2015.03.26.jar;C:\Users\Luís Puhl\.ivy2\cache\nz.ac.waikato.cms.weka.thirdparty\bounce\jars\bounce-0.18.jar;C:\Users\Luís Puhl\.ivy2\cache\nz.ac.waikato.cms.weka\weka-dev\jars\weka-dev-3.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\nz.ac.waikato.cms.moa\moa\jars\moa-2019.05.0.jar;C:\Users\Luís Puhl\.ivy2\cache\net.sourceforge.f2j\arpack_combined_all\jars\arpack_combined_all-0.1.jar;C:\Users\Luís Puhl\.ivy2\cache\net.sf.trove4j\trove4j\jars\trove4j-3.0.3.jar;C:\Users\Luís Puhl\.ivy2\cache\net.sf.meka.thirdparty\mulan\jars\mulan-1.4.0.jar;C:\Users\Luís Puhl\.ivy2\cache\net.sf.meka.thirdparty\mst\jars\mst-4.0.jar;C:\Users\Luís Puhl\.ivy2\cache\net.sf.meka\meka\jars\meka-1.9.2.jar;C:\Users\Luís Puhl\.ivy2\cache\junit\junit\jars\junit-3.8.1.jar;C:\Users\Luís Puhl\.ivy2\cache\gov.nist.math\jama\jars\jama-1.0.3.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-logging\commons-logging\jars\commons-logging-1.2.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-cli\commons-cli\jars\commons-cli-1.4.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-beanutils\commons-beanutils\jars\commons-beanutils-1.9.3.jar;C:\Users\Luís Puhl\.ivy2\cache\com.opencsv\opencsv\jars\opencsv-4.0.jar;C:\Users\Luís Puhl\.ivy2\cache\com.jidesoft\jide-oss\jars\jide-oss-3.6.18.jar;C:\Users\Luís Puhl\.ivy2\cache\com.googlecode.netlib-java\netlib-java\jars\netlib-java-1.1.jar;C:\Users\Luís Puhl\.ivy2\cache\com.googlecode.matrix-toolkits-java\mtj\jars\mtj-1.0.4.jar;C:\Users\Luís Puhl\.ivy2\cache\com.googlecode.efficient-java-matrix-library\ejml\jars\ejml-0.22.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.waikato\jclasslocator\jars\jclasslocator-0.0.12.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.waikato\fcms-widgets\jars\fcms-widgets-0.0.14.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fracpete\sizeofag\jars\sizeofag-1.0.4.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fracpete\processoutput4j\jars\processoutput4j-0.0.7.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fracpete\multisearch-weka-package\jars\multisearch-weka-package-2017.10.1.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fracpete\jshell-scripting\jars\jshell-scripting-0.0.1.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fracpete\jfilechooser-bookmarks\jars\jfilechooser-bookmarks-0.1.6.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fracpete\jclipboardhelper\jars\jclipboardhelper-0.1.1.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_system-win-x86_64\jars\netlib-native_system-win-x86_64-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_system-win-i686\jars\netlib-native_system-win-i686-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_system-osx-x86_64\jars\netlib-native_system-osx-x86_64-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_system-linux-x86_64\jars\netlib-native_system-linux-x86_64-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_system-linux-i686\jars\netlib-native_system-linux-i686-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_system-linux-armhf\jars\netlib-native_system-linux-armhf-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_ref-win-x86_64\jars\netlib-native_ref-win-x86_64-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_ref-win-i686\jars\netlib-native_ref-win-i686-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_ref-osx-x86_64\jars\netlib-native_ref-osx-x86_64-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_ref-linux-x86_64\jars\netlib-native_ref-linux-x86_64-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_ref-linux-i686\jars\netlib-native_ref-linux-i686-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\netlib-native_ref-linux-armhf\jars\netlib-native_ref-linux-armhf-1.1-natives.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\native_system-java\jars\native_system-java-1.1.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\native_ref-java\jars\native_ref-java-1.1.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil.netlib\core\jars\core-1.1.2.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.fommil\jniloader\jars\jniloader-1.1.jar;C:\Users\Luís Puhl\.ivy2\cache\com.fifesoft\rsyntaxtextarea\jars\rsyntaxtextarea-2.6.1.jar;C:\Users\Luís Puhl\.ivy2\cache\classworlds\classworlds\jars\classworlds-1.1-alpha-2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.slf4j\slf4j-log4j12\jars\slf4j-log4j12-1.7.29.jar;C:\Users\Luís Puhl\.ivy2\cache\org.slf4j\slf4j-api\jars\slf4j-api-1.7.29.jar;C:\Users\Luís Puhl\.ivy2\cache\org.scala-lang.modules\scala-xml_2.11\bundles\scala-xml_2.11-1.0.5.jar;C:\Users\Luís Puhl\.ivy2\cache\org.scala-lang.modules\scala-parser-combinators_2.11\bundles\scala-parser-combinators_2.11-1.1.1.jar;C:\Users\Luís Puhl\.ivy2\cache\org.scala-lang.modules\scala-java8-compat_2.11\bundles\scala-java8-compat_2.11-0.7.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.scala-lang\scala-reflect\jars\scala-reflect-2.11.12.jar;C:\Users\Luís Puhl\.ivy2\cache\org.scala-lang\scala-library\jars\scala-library-2.11.12.jar;C:\Users\Luís Puhl\.ivy2\cache\org.scala-lang\scala-compiler\jars\scala-compiler-2.11.12.jar;C:\Users\Luís Puhl\.ivy2\cache\org.reactivestreams\reactive-streams\jars\reactive-streams-1.0.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.objenesis\objenesis\jars\objenesis-2.1.jar;C:\Users\Luís Puhl\.ivy2\cache\org.javassist\javassist\bundles\javassist-3.19.0-GA.jar;C:\Users\Luís Puhl\.ivy2\cache\org.clapper\grizzled-slf4j_2.11\jars\grizzled-slf4j_2.11-1.3.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-shaded-netty\jars\flink-shaded-netty-4.1.32.Final-7.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-shaded-guava\jars\flink-shaded-guava-18.0-7.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.flink\flink-shaded-asm-6\jars\flink-shaded-asm-6-6.2.1-7.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.commons\commons-compress\jars\commons-compress-1.18.jar;C:\Users\Luís Puhl\.ivy2\cache\log4j\log4j\bundles\log4j-1.2.17.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-io\commons-io\jars\commons-io-2.4.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-collections\commons-collections\jars\commons-collections-3.2.2.jar;C:\Users\Luís Puhl\.ivy2\cache\com.typesafe.akka\akka-stream_2.11\jars\akka-stream_2.11-2.5.21.jar;C:\Users\Luís Puhl\.ivy2\cache\com.typesafe.akka\akka-slf4j_2.11\jars\akka-slf4j_2.11-2.5.21.jar;C:\Users\Luís Puhl\.ivy2\cache\com.typesafe.akka\akka-protobuf_2.11\jars\akka-protobuf_2.11-2.5.21.jar;C:\Users\Luís Puhl\.ivy2\cache\com.typesafe.akka\akka-actor_2.11\jars\akka-actor_2.11-2.5.21.jar;C:\Users\Luís Puhl\.ivy2\cache\com.typesafe\ssl-config-core_2.11\bundles\ssl-config-core_2.11-0.3.7.jar;C:\Users\Luís Puhl\.ivy2\cache\com.typesafe\config\bundles\config-1.3.3.jar;C:\Users\Luís Puhl\.ivy2\cache\com.twitter\chill_2.11\jars\chill_2.11-0.7.6.jar;C:\Users\Luís Puhl\.ivy2\cache\com.twitter\chill-java\jars\chill-java-0.7.6.jar;C:\Users\Luís Puhl\.ivy2\cache\com.google.code.findbugs\jsr305\jars\jsr305-1.3.9.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.scopt\scopt_2.11\jars\scopt_2.11-3.5.0.jar;C:\Users\Luís Puhl\.ivy2\cache\com.esotericsoftware.minlog\minlog\jars\minlog-1.2.jar;C:\Users\Luís Puhl\.ivy2\cache\asm\asm\jars\asm-3.1.jar;C:\Users\Luís Puhl\.ivy2\cache\com.dreizak\miniball\jars\miniball-1.0.3.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.javacliparser\javacliparser\jars\javacliparser-0.5.0.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.vbmacher\java-cup\jars\java-cup-11b-20160615.jar;C:\Users\Luís Puhl\.ivy2\cache\com.github.vbmacher\java-cup-runtime\jars\java-cup-runtime-11b-20160615.jar;C:\Users\Luís Puhl\.ivy2\cache\com.google.code.gson\gson\jars\gson-2.2.4.jar;C:\Users\Luís Puhl\.ivy2\cache\com.google.guava\guava\bundles\guava-17.0.jar;C:\Users\Luís Puhl\.ivy2\cache\com.google.protobuf\protobuf-java\bundles\protobuf-java-2.5.0.jar;C:\Users\Luís Puhl\.ivy2\cache\com.jamesmurty.utils\java-xmlbuilder\jars\java-xmlbuilder-0.4.jar;C:\Users\Luís Puhl\.ivy2\cache\com.jcraft\jsch\jars\jsch-0.1.42.jar;C:\Users\Luís Puhl\.ivy2\cache\com.sun.istack\istack-commons-runtime\jars\istack-commons-runtime-3.0.8.jar;C:\Users\Luís Puhl\.ivy2\cache\com.sun.jersey\jersey-core\bundles\jersey-core-1.9.jar;C:\Users\Luís Puhl\.ivy2\cache\com.sun.jersey\jersey-json\bundles\jersey-json-1.9.jar;C:\Users\Luís Puhl\.ivy2\cache\com.sun.jersey\jersey-server\bundles\jersey-server-1.9.jar;C:\Users\Luís Puhl\.ivy2\cache\com.sun.xml.bind\jaxb-impl\jars\jaxb-impl-2.2.3-1.jar;C:\Users\Luís Puhl\.ivy2\cache\com.sun.xml.fastinfoset\FastInfoset\jars\FastInfoset-1.2.16.jar;C:\Users\Luís Puhl\.ivy2\cache\com.thoughtworks.paranamer\paranamer\jars\paranamer-2.3.jar;C:\Users\Luís Puhl\.ivy2\cache\com.yammer.metrics\metrics-core\jars\metrics-core-2.2.0.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-beanutils\commons-beanutils-core\jars\commons-beanutils-core-1.8.0.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-codec\commons-codec\jars\commons-codec-1.4.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-configuration\commons-configuration\jars\commons-configuration-1.6.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-daemon\commons-daemon\jars\commons-daemon-1.0.13.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-digester\commons-digester\jars\commons-digester-1.8.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-el\commons-el\jars\commons-el-1.0.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-httpclient\commons-httpclient\jars\commons-httpclient-3.1.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-lang\commons-lang\jars\commons-lang-2.6.jar;C:\Users\Luís Puhl\.ivy2\cache\commons-net\commons-net\jars\commons-net-3.1.jar;C:\Users\Luís Puhl\.ivy2\cache\io.netty\netty\bundles\netty-3.7.0.Final.jar;C:\Users\Luís Puhl\.ivy2\cache\jakarta.activation\jakarta.activation-api\jars\jakarta.activation-api-1.2.1.jar;C:\Users\Luís Puhl\.ivy2\cache\jakarta.xml.bind\jakarta.xml.bind-api\jars\jakarta.xml.bind-api-2.3.2.jar;C:\Users\Luís Puhl\.ivy2\cache\javax.activation\activation\jars\activation-1.1.jar;C:\Users\Luís Puhl\.ivy2\cache\javax.servlet\servlet-api\jars\servlet-api-2.5.jar;C:\Users\Luís Puhl\.ivy2\cache\javax.servlet.jsp\jsp-api\jars\jsp-api-2.1.jar;C:\Users\Luís Puhl\.ivy2\cache\javax.xml.bind\jaxb-api\jars\jaxb-api-2.2.2.jar;C:\Users\Luís Puhl\.ivy2\cache\javax.xml.stream\stax-api\jars\stax-api-1.0-2.jar;C:\Users\Luís Puhl\.ivy2\cache\jline\jline\jars\jline-0.9.94.jar;C:\Users\Luís Puhl\.ivy2\cache\net.java.dev.jets3t\jets3t\jars\jets3t-0.9.0.jar;C:\Users\Luís Puhl\.ivy2\cache\net.jcip\jcip-annotations\jars\jcip-annotations-1.0.jar;C:\Users\Luís Puhl\.ivy2\cache\nz.ac.waikato.cms.weka\weka-stable\jars\weka-stable-3.8.4.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.avro\avro\bundles\avro-1.7.7.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.curator\curator-client\bundles\curator-client-2.6.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.curator\curator-framework\bundles\curator-framework-2.6.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.curator\curator-recipes\bundles\curator-recipes-2.6.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.directory.api\api-asn1-api\bundles\api-asn1-api-1.0.0-M20.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.directory.api\api-util\bundles\api-util-1.0.0-M20.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.directory.server\apacheds-i18n\bundles\apacheds-i18n-2.0.0-M15.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.directory.server\apacheds-kerberos-codec\bundles\apacheds-kerberos-codec-2.0.0-M15.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.hadoop\hadoop-annotations\jars\hadoop-annotations-2.6.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.hadoop\hadoop-auth\jars\hadoop-auth-2.6.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.hadoop\hadoop-common\jars\hadoop-common-2.6.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.hadoop\hadoop-hdfs\jars\hadoop-hdfs-2.6.0.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.httpcomponents\httpclient\jars\httpclient-4.2.5.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.httpcomponents\httpcore\jars\httpcore-4.2.4.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.samoa\samoa-api\jars\samoa-api-0.4.0-incubating.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.samoa\samoa-instances\jars\samoa-instances-0.4.0-incubating.jar;C:\Users\Luís Puhl\.ivy2\cache\org.apache.zookeeper\zookeeper\jars\zookeeper-3.4.6.jar;C:\Users\Luís Puhl\.ivy2\cache\org.codehaus.jackson\jackson-core-asl\jars\jackson-core-asl-1.9.13.jar;C:\Users\Luís Puhl\.ivy2\cache\org.codehaus.jackson\jackson-jaxrs\jars\jackson-jaxrs-1.8.3.jar;C:\Users\Luís Puhl\.ivy2\cache\org.codehaus.jackson\jackson-mapper-asl\jars\jackson-mapper-asl-1.9.13.jar;C:\Users\Luís Puhl\.ivy2\cache\org.codehaus.jackson\jackson-xc\jars\jackson-xc-1.8.3.jar;C:\Users\Luís Puhl\.ivy2\cache\org.codehaus.jettison\jettison\bundles\jettison-1.1.jar;C:\Users\Luís Puhl\.ivy2\cache\org.glassfish.jaxb\jaxb-runtime\jars\jaxb-runtime-2.3.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.glassfish.jaxb\txw2\jars\txw2-2.3.2.jar;C:\Users\Luís Puhl\.ivy2\cache\org.htrace\htrace-core\jars\htrace-core-3.0.4.jar;C:\Users\Luís Puhl\.ivy2\cache\org.jvnet.staxex\stax-ex\jars\stax-ex-1.8.1.jar;C:\Users\Luís Puhl\.ivy2\cache\org.mortbay.jetty\jetty\jars\jetty-6.1.26.jar;C:\Users\Luís Puhl\.ivy2\cache\org.mortbay.jetty\jetty-util\jars\jetty-util-6.1.26.jar;C:\Users\Luís Puhl\.ivy2\cache\tomcat\jasper-compiler\jars\jasper-compiler-5.5.23.jar;C:\Users\Luís Puhl\.ivy2\cache\tomcat\jasper-runtime\jars\jasper-runtime-5.5.23.jar;C:\Users\Luís Puhl\.ivy2\cache\xerces\xercesImpl\jars\xercesImpl-2.9.1.jar;C:\Users\Luís Puhl\.ivy2\cache\xml-apis\xml-apis\jars\xml-apis-1.3.04.jar;C:\Users\Luís Puhl\.ivy2\cache\xmlenc\xmlenc\jars\xmlenc-0.52.jar;C:\Users\Luís Puhl\.ivy2\cache\org.json\json\bundles\json-20190722.jar" br.ufscar.dc.ppgcc.gsdr.mfog.Classifier
   * 14:16:08 INFO  mfog.Classifier$ jobName = br.ufscar.dc.ppgcc.gsdr.mfog.Classifier$
   * 14:18:05 INFO  mfog.Classifier$ setup in 116.757s
   * 14:23:40 INFO  mfog.Classifier$ Ran par=1 in 325.084s
   * 14:26:46 INFO  mfog.Classifier$ Ran par=2 in 184.172s
   * 14:28:59 INFO  mfog.Classifier$ Ran par=3 in 130.406s
   * 14:30:52 INFO  mfog.Classifier$ Ran par=4 in 110.994s
   * 14:32:35 INFO  mfog.Classifier$ Ran par=5 in 100.105s
   * 14:34:04 INFO  mfog.Classifier$ Ran par=6 in 86.264s
   * 14:35:29 INFO  mfog.Classifier$ Ran par=7 in 82.671s
   * 14:37:04 INFO  mfog.Classifier$ Ran par=8 in 93.032s
   */
  def runWithFullStatic(outDir: String, jobName: String, par: Int, model: immutable.Seq[Cluster], data: immutable.Seq[Point]): Unit = {
    class R(val p: Point, val c: Cluster, val d: Double, val l: String, val t: Long) {
      override def toString: String = s"$d,$l,$t,$p,$c"
    }
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment
    env.setParallelism(par)

    val outStream = env
      .fromCollection(data)
      .connect(env.fromElements[Seq[Cluster]](model).broadcast())
      .process(new BroadcastProcessFunction[Point, Seq[Cluster], R]() {
        type ReadOnlyContext = BroadcastProcessFunction[Point, Seq[Cluster], R]#ReadOnlyContext
        type Context = BroadcastProcessFunction[Point, Seq[Cluster], R]#Context
        var model: mutable.Buffer[Cluster] = mutable.Buffer.empty
        var backlog: mutable.Buffer[Point] = mutable.Buffer.empty

        def classify(x: Point): R = {
          val (d, c) = model.map(c => (x.euclideanDistance(c.center), c)).minBy(_._1)
          val l = if (d <= c.variance) c.label else "unk"
          new R(x, c, d, l, System.currentTimeMillis() - x.time)
        }

        def processElement(x: Point, ctx: ReadOnlyContext, out: Collector[R]): Unit = {
          if (model.isEmpty) {
            backlog.append(x)
          } else {
            out.collect(classify(x))
          }
        }

        def processBroadcastElement(value: Seq[Cluster], ctx: Context, out: Collector[R]): Unit = {
          if (value.nonEmpty) {
            model = mutable.Buffer.empty
            model.appendAll(value)
            if (backlog.nonEmpty) {
              backlog.foreach(x => out.collect(classify(x)))
            }
          }
        }
      })
    outStream.writeAsText(s"$outDir/classifier-$par")
    val latency = outStream.map(_.t)
    // latency.writeAsText(s"$outDir/latency-$par")
    val windowSize: Long = 10000
    latency
      .countWindowAll(windowSize)
      .reduce((a, b) => a + b)
      .map(_ / windowSize)
      .writeAsText(s"$outDir/latency-avg-$par")

    // LOG.info(s"Ready to run with par=$par")
    val start = System.currentTimeMillis()
    env.execute(jobName)
    LOG.info(s"Ran par=$par in ${(System.currentTimeMillis() - start) * 10e-4}s")
  }

  /**
   * 15:59:55 INFO  mfog.Classifier$ setup in 93.898s
   * 16:01:29 INFO  mfog.Classifier$ Ran runWithStaticModel par=8 in 87.83s
   * 16:03:01 INFO  mfog.Classifier$ Ran runWithStaticModel par=7 in 91.461s
   * 16:04:34 INFO  mfog.Classifier$ Ran runWithStaticModel par=6 in 93.796s
   * 16:06:22 INFO  mfog.Classifier$ Ran runWithStaticModel par=5 in 107.389s
   * 16:08:14 INFO  mfog.Classifier$ Ran runWithStaticModel par=4 in 112.238s
   * 16:10:30 INFO  mfog.Classifier$ Ran runWithStaticModel par=3 in 136.213s
   * 16:13:36 INFO  mfog.Classifier$ Ran runWithStaticModel par=2 in 185.903s
   * 16:19:25 INFO  mfog.Classifier$ Ran runWithStaticModel par=1 in 348.925s
   *
   * @param outDir
   * @param jobName
   * @param par
   * @param model
   */
  def runWithStaticModel(outDir: String, jobName: String, par: Int,  model: immutable.Seq[Cluster]): Unit = {
    class R(val p: Point, val c: Cluster, val d: Double, val l: String, val t: Long) {
      override def toString: String = s"$d,$l,$t,$p,$c"
    }
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment
    env.setParallelism(par)

    val outStream = env.socketTextStream("localhost", 9996)
      .map[Point]((x: String) => Point.fromJson(x))
      .connect(env.fromElements[Seq[Cluster]](model).broadcast())
      .process(new BroadcastProcessFunction[Point, Seq[Cluster], R]() {
        type ReadOnlyContext = BroadcastProcessFunction[Point, Seq[Cluster], R]#ReadOnlyContext
        type Context = BroadcastProcessFunction[Point, Seq[Cluster], R]#Context
        var model: mutable.Buffer[Cluster] = mutable.Buffer.empty
        var backlog: mutable.Buffer[Point] = mutable.Buffer.empty

        def classify(x: Point): R = {
          val (d, c) = model.map(c => (x.euclideanDistance(c.center), c)).minBy(_._1)
          val l = if (d <= c.variance) c.label else "unk"
          new R(x, c, d, l, System.currentTimeMillis() - x.time)
        }

        def processElement(x: Point, ctx: ReadOnlyContext, out: Collector[R]): Unit = {
          if (model.isEmpty) {
            backlog.append(x)
          } else {
            out.collect(classify(x))
          }
        }

        def processBroadcastElement(value: Seq[Cluster], ctx: Context, out: Collector[R]): Unit = {
          if (value.nonEmpty) {
            model = mutable.Buffer.empty
            model.appendAll(value)
            if (backlog.nonEmpty) {
              backlog.foreach(x => out.collect(classify(x)))
            }
          }
        }
      })
    outStream.writeAsText(s"$outDir/classifier-$par")
    val latency = outStream.map(_.t)
    latency.writeAsText(s"$outDir/latency-$par")
    val windowSize: Long = 10000
    latency
      .countWindowAll(windowSize)
      .reduce((a, b) => a + b)
      .map(_ / windowSize)
      .writeAsText(s"$outDir/latency-avg-$par")

    val start = System.currentTimeMillis()
    env.execute(jobName)
    val end = System.currentTimeMillis()
    LOG.info(s"Ran runWithStaticModel par=$par in ${(end - start) * 10e-4}s")
  }

  /**
   * 14:54:05 INFO  mfog.Classifier$ Ready to run baseline
   * 14:55:57 INFO  mfog.Classifier$ Ran baseline in 111.805s
   *
   * @param outDir
   * @param jobName
   */
  def baseline(outDir: String, jobName: String): Unit = {
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment

    env.socketTextStream("localhost", 9996)
      .map[Point](new MapFunction[String, Point]() {
        override def map(value: String): Point = Point.fromJson(value)
      })
      .connect(
        env.socketTextStream("localhost", 9997)
          .map[Seq[Cluster]](new RichMapFunction[String, Seq[Cluster]]() {
            var model: mutable.Buffer[Cluster] = mutable.Buffer.empty

            override def open(parameters: Configuration): Unit = {
              super.open(parameters)
              model = mutable.Buffer.empty
            }

            override def map(value: String): Seq[Cluster] = {
              val cl = Cluster.fromJson(new JSONObject(value))
              model.append(cl)
              model
            }
          })
          .broadcast()
      )
      .process(new BroadcastProcessFunction[Point, Seq[Cluster], (Long, Long)]() {
        // type Context = CoProcessFunction[Point, Seq[Cluster], (Long, Long)]#Context
        type ReadOnlyContext = BroadcastProcessFunction[Point, Seq[Cluster], (Long, Long)]#ReadOnlyContext
        type Context = BroadcastProcessFunction[Point, Seq[Cluster], (Long, Long)]#Context

        override def processElement(x: Point, ctx: ReadOnlyContext, out: Collector[(Long, Long)]): Unit = {
          out.collect((System.currentTimeMillis() - x.time, 0))
        }

        override def processBroadcastElement(value: Seq[Cluster], ctx: Context, out: Collector[(Long, Long)]): Unit = {
          out.collect((0, System.currentTimeMillis() - value.last.time))
        }
      })
      .writeAsText(s"$outDir/baseline")
    LOG.info(s"Ready to run baseline")
    val start = System.currentTimeMillis()
    env.execute(jobName)
    val end = System.currentTimeMillis()
    LOG.info(s"Ran baseline in ${(end - start) * 10e-4}s")
  }

  /**
   * 15:33:20 INFO  mfog.Classifier$ Ran par=8 in 101.63s
   * 15:34:48 INFO  mfog.Classifier$ Ran par=7 in 87.35300000000001s
   * 15:36:17 INFO  mfog.Classifier$ Ran par=6 in 89.44200000000001s
   * 15:37:47 INFO  mfog.Classifier$ Ran par=5 in 89.737s
   * 15:39:16 INFO  mfog.Classifier$ Ran par=4 in 89.217s
   * 15:40:41 INFO  mfog.Classifier$ Ran par=3 in 84.538s
   * 15:42:36 INFO  mfog.Classifier$ Ran par=2 in 115.894s
   * 15:47:59 INFO  mfog.Classifier$ Ran par=1 in 322.672s
   *
   * @param outDir
   * @param jobName
   * @param par
   */
  def run(outDir: String, jobName: String, par: Int): Unit = {
    class R(val p: Point, val c: Cluster, val d: Double, val l: String, val t: Long) {
      override def toString: String = s"$d,$l,$t,$p,$c"
    }
    val env: StreamExecutionEnvironment = StreamExecutionEnvironment.getExecutionEnvironment
    env.setParallelism(par)

    val outStream = env.socketTextStream("localhost", 9996)
      .map[Point]((x: String) => Point.fromJson(x))
      .connect(env.socketTextStream("localhost", 9997)
        .map[Seq[Cluster]](new RichMapFunction[String, Seq[Cluster]]() {
          var model: mutable.Buffer[Cluster] = mutable.Buffer.empty

          override def open(parameters: Configuration): Unit = {
            super.open(parameters)
            model = mutable.Buffer.empty
          }

          override def map(value: String): Seq[Cluster] = {
            val cl = Cluster.fromJson(new JSONObject(value))
            model.append(cl)
            model
          }
        })
        .broadcast()
      )
      .process(new BroadcastProcessFunction[Point, Seq[Cluster], R]() {
        type ReadOnlyContext = BroadcastProcessFunction[Point, Seq[Cluster], R]#ReadOnlyContext
        type Context = BroadcastProcessFunction[Point, Seq[Cluster], R]#Context
        var model: mutable.Buffer[Cluster] = mutable.Buffer.empty
        var backlog: mutable.Buffer[Point] = mutable.Buffer.empty

        def classify(x: Point): R = {
          val (d, c) = model.map(c => (x.euclideanDistance(c.center), c)).minBy(_._1)
          val l = if (d <= c.variance) c.label else "unk"
          new R(x, c, d, l, System.currentTimeMillis() - x.time)
        }

        def processElement(x: Point, ctx: ReadOnlyContext, out: Collector[R]): Unit = {
          if (model.isEmpty) {
            backlog.append(x)
          } else {
            out.collect(classify(x))
          }
        }

         def processBroadcastElement(value: Seq[Cluster], ctx: Context, out: Collector[R]): Unit = {
          if (value.nonEmpty) {
            model = mutable.Buffer.empty
            model.appendAll(value)
            if (backlog.nonEmpty) {
              backlog.foreach(x => out.collect(classify(x)))
            }
          }
        }
      })
    outStream.writeAsText(s"$outDir/classifier-$par")
    val latency = outStream.map(_.t)
    latency.writeAsText(s"$outDir/latency-$par")
    val windowSize: Long = 10000
    latency
      .countWindowAll(windowSize)
      .reduce((a, b) => a + b)
      .map(_ / windowSize)
      .writeAsText(s"$outDir/latency-avg-$par")

    val start = System.currentTimeMillis()
    env.execute(jobName)
    val end = System.currentTimeMillis()
    LOG.info(s"Ran par=$par in ${(end - start) * 10e-4}s")
  }

    /**
     - usar somente um modelo sem atualização (MVP)
     - tabela de tempos por parametro de paralelismo
        * minas leva 54s (online)
     - mover para os RPI para extração de tempo também.
    x1, x2, x3 ...
    t1, t2, t3 ...
                        |-> 8 processos
    m1, m2, m3 ...
    tm1, tm2, tm3 ...

    |x1, model|
    */
}
