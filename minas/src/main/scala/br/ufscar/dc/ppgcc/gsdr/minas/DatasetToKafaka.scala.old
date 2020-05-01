package br.ufscar.dc.ppgcc.gsdr.minas

import java.io.{BufferedWriter, File, FileInputStream, FileWriter}
import java.lang
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter
import java.util.Properties
import java.util.function.BiConsumer

import org.apache.flink.streaming.api.scala._
import org.apache.flink.streaming.api.scala.StreamExecutionEnvironment
import org.apache.flink.streaming.connectors.kafka.{FlinkKafkaProducer, KafkaDeserializationSchema, KafkaSerializationSchema}

import org.apache.flink.api.common.typeinfo.TypeInformation
import org.apache.flink.api.common.serialization.SimpleStringSchema
import org.apache.flink.api.scala.typeutils.Types
import org.apache.flink.api.java.utils.ParameterTool

import org.apache.kafka.clients.consumer.ConsumerRecord
import org.apache.kafka.clients.producer.ProducerRecord

import scala.collection.JavaConversions._
import scala.util.Try

object DatasetToKafaka {
  final val FILENAME_OFFLINE_KEY = "filenameOffline"
  final val FILENAME_ONLINE_KEY = "filenameOnline"
  final val OUTPUT_DIRECTORY_KEY = "outputDirectory"
  final val DEFAULT_TOPIC: String = "test"

  @scala.annotation.tailrec
  def getDirectory(outDir: String): String = {
    val outDirClean = outDir.replaceAll("[/\\\\]+", File.separator)
    val outputDirectorySrc = if (!outDirClean.endsWith(File.separator)) outDirClean + File.separator else outDirClean
    val dateString = LocalDateTime.now.format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-")
    val outputDirectory = outputDirectorySrc + dateString + File.separator
    val dir = new File(outputDirectory)
    if (!dir.exists) {
      if (!dir.mkdirs) throw new RuntimeException(s"Output directory '$outputDirectory'could not be created.")
      else outputDirectory
    } else getDirectory(outputDirectorySrc)
  }

  class SerDes extends KafkaSerializationSchema[String] with KafkaDeserializationSchema[String] {
    val simpleStringSchema = new SimpleStringSchema()

    override def serialize(element: String, timestamp: lang.Long): ProducerRecord[Array[Byte], Array[Byte]] =
      new ProducerRecord[Array[Byte], Array[Byte]](DEFAULT_TOPIC, simpleStringSchema.serialize(element));

    override def isEndOfStream(nextElement: String): Boolean = simpleStringSchema.isEndOfStream(nextElement)

    override def deserialize(record: ConsumerRecord[Array[Byte], Array[Byte]]): String = simpleStringSchema.deserialize(record.value())

    override def getProducedType: TypeInformation[String] = Types.STRING
  }

  def main__(args: Array[String]): Unit = {
    var params = Map(
      FILENAME_OFFLINE_KEY -> FILENAME_OFFLINE_KEY,
      FILENAME_ONLINE_KEY -> FILENAME_ONLINE_KEY,
      OUTPUT_DIRECTORY_KEY -> OUTPUT_DIRECTORY_KEY
    )
    //
    val propsFileName: String = System.getProperty("user.dir") + "/src/main/resources/minas.properties".replace("/", File.separator);
    val config = new Properties
    Try {
      val propsFile = new FileInputStream(propsFileName)
      config.load(propsFile)
      propsFile.close()
    }
    for (confKey <- config.stringPropertyNames) {
      params = params + (confKey -> config.getProperty(confKey))
    }
    //
    val parameterTool = ParameterTool.fromArgs(args)
    parameterTool.toMap.forEach(new BiConsumer[String, String] {
      override def accept(key: String, value: String): Unit = {
//        if (!arg.startsWith("--")) throw new IllegalArgumentException(s"Error at argument '$arg'. Expected format '--arg=value'.")
//        val split = arg.replaceFirst("--", "").split("=").toSeq
//        val (key: String, value: String) = split match {
//          case key :: value :: tail => (key, value)
//          case _ => throw new IllegalArgumentException(s"Error at argument '$arg'. Expected format '--arg=value'.")
//        }
        if (!params.containsKey(key)) throw new IllegalArgumentException(s"Argument '$key' is not known.")
        params = params + (key -> value)
      }
    })
//    if (args != null) args.foreach(arg =>
    //
    val outputDirectory: String = getDirectory(params("outputDirectory"))
    params = params + ("outputDirectory" -> outputDirectory)
    val configWriter = new BufferedWriter(new FileWriter(new File(outputDirectory + "arguments.properties")))
    params.foreach{ case (key, value) => configWriter.write(key + "=" + value + "\n") }
    configWriter.write("\n")
    configWriter.close()
    //
    // val myProducer = new FlinkKafkaProducer[String]("localhost:9092","test", new SimpleStringSchema)
    // @deprecated use {@link #FlinkKafkaProducer(String, KafkaSerializationSchema, Properties, FlinkKafkaProducer.Semantic)}
    /*
      public FlinkKafkaProducer(
      String defaultTopic,
      KafkaSerializationSchema<IN> serializationSchema,
      Properties producerConfig,
      FlinkKafkaProducer.Semantic semantic) {
     */
    val props: Properties = new Properties
    props.setProperty("bootstrap.servers", "localhost:9092")
    props.setProperty("acks", "all")
    props.setProperty("group.id", "customerAnalytics");
    // props.setProperty("key.serializer", "org.apache.kafka.common.serialization.StringSerializer")
    // props.setProperty("value.serializer", "org.apache.kafka.common.serialization.StringSerializer")
    val semantic = FlinkKafkaProducer.Semantic.AT_LEAST_ONCE
    // FlinkKafkaConsumer<String> kafkaSource = new FlinkKafkaConsumer<>("customer.create", new SimpleStringSchema(), properties);
    val myProducer = new FlinkKafkaProducer[String](DEFAULT_TOPIC, new SerDes, props, semantic)
    myProducer.setWriteTimestampToKafka(true)
    //
    val env = StreamExecutionEnvironment.getExecutionEnvironment
    val offlineFile = env.readTextFile(params(FILENAME_OFFLINE_KEY))
    offlineFile.addSink(myProducer)
    env.execute(getClass.toGenericString)
  }
}
