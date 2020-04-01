ThisBuild / resolvers ++= Seq(
    "Apache Development Snapshot Repository" at "https://repository.apache.org/content/repositories/snapshots/",
    Resolver.mavenLocal
)

name := "Minas"

version := "0.1"

organization := "br.ufscar.dc.ppgcc.gsdr.minas"

ThisBuild / scalaVersion := "2.11.12"

val flinkVersion = "1.9.1"

val flinkDependencies = Seq(
  "org.apache.flink" %% "flink-scala" % flinkVersion, // % "provided",
  "org.apache.flink" %% "flink-streaming-scala" % flinkVersion) // % "provided"
  // "org.apache.flink" %% "flink-connector-kafka_2.11" % flinkVersion

val samoaVersion = "0.4.0-incubating"

val loggerDependencies = Seq(
  "log4j" % "log4j" % "1.2.17",
  "org.slf4j" % "slf4j-log4j12" % "1.7.29")
val minasDependencies = Seq(
  "nz.ac.waikato.cms.moa" % "moa" % "2019.05.0",
  "nz.ac.waikato.cms.weka" % "weka-stable" % "3.8.4",
  "org.apache.samoa" % "samoa" % samoaVersion,
  "org.apache.samoa" % "samoa-instances" % samoaVersion,
  "org.apache.samoa" % "samoa-api" % samoaVersion,
)
val testDependencies = Seq(
  "org.scalactic" %% "scalactic" % "3.1.0",
  "org.scalatest" %% "scalatest" % "3.1.0" % "test")

// https://repo1.maven.org/maven2/org/apache/flink/flink-connector-kafka_2.11/1.9.1/flink-connector-kafka_2.11-1.9.1.pom
// https://repo1.maven.org/maven2/org/apache/flink/flink-connector-kafka/1.9.1/flink-connector-kafka-1.9.1.pom
// https://repo1.maven.org/maven2/org/apache/flink/flink-connector-kafka/1.9.1/flink-connector-kafka-1.9.1.pom

lazy val root = (project in file(".")).
  settings(
    libraryDependencies ++= flinkDependencies,
    libraryDependencies += "org.apache.flink" % "flink-connector-kafka_2.11" % "1.9.1",
    libraryDependencies ++= loggerDependencies,
    libraryDependencies ++= minasDependencies
  )

assembly / mainClass := Some("br.ufscar.dc.ppgcc.gsdr.minas.MinasFlinkOffline")

// make run command include the provided dependencies
Compile / run  := Defaults.runTask(Compile / fullClasspath,
                                   Compile / run / mainClass,
                                   Compile / run / runner
                                  ).evaluated

// stays inside the sbt console when we press "ctrl-c" while a Flink programme executes with "run" or "runMain"
Compile / run / fork := true
Global / cancelable := true

// exclude Scala library from assembly
// assembly / assemblyOption  := (assembly / assemblyOption).value.copy(includeScala = false)
