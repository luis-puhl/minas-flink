ThisBuild / resolvers ++= Seq(
    "Apache Development Snapshot Repository" at "https://repository.apache.org/content/repositories/snapshots/",
    Resolver.mavenLocal
)

name := "sbt-flink"
version := "0.1-SNAPSHOT"
organization := "br.ufscar.dc.ppgcc.gsdr.minas"

ThisBuild / scalaVersion := "2.11.12"

val flinkVersion = "1.9.1"

// val flinkDependencies = Seq(
//   "org.apache.flink" %% "flink-scala" % flinkVersion % "provided",
//   "org.apache.flink" %% "flink-streaming-scala" % flinkVersion % "provided")
val flinkDependencies = Seq(
  "org.apache.flink" %% "flink-scala" % flinkVersion,
  "org.apache.flink" %% "flink-streaming-scala" % flinkVersion)

val loggerDependencies = Seq(
  "log4j" % "log4j" % "1.2.17",
  "org.slf4j" % "slf4j-log4j12" % "1.7.29")

lazy val root = (project in file(".")).
  settings(
    libraryDependencies ++= flinkDependencies,
    libraryDependencies ++= loggerDependencies//,
    //libraryDependencies += "nz.ac.waikato.cms.moa" % "moa" % "2019.05.0"
  )

assembly / mainClass := Some("br.ufscar.dc.ppgcc.gsdr.minas.KMeansVector")

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

fork in run := true
