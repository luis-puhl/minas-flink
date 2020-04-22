ThisBuild / resolvers ++= Seq(
    "Apache Development Snapshot Repository" at "https://repository.apache.org/content/repositories/snapshots/",
    Resolver.mavenLocal
)

name := "Minas"
version := "0.2"
organization := "br.ufscar.dc.ppgcc.gsdr.minas"

ThisBuild / scalaVersion := "2.11.12"

val flinkVersion = "1.9.2"
val samoaVersion = "0.4.0-incubating"

lazy val root = (project in file(".")).
  settings(
    libraryDependencies += "org.apache.flink" %% "flink-scala" % flinkVersion, // % "provided",
    libraryDependencies += "org.apache.flink" %% "flink-streaming-scala" % flinkVersion, // % "provided"
    libraryDependencies += "org.apache.flink" %% "flink-connector-kafka" % flinkVersion,
    libraryDependencies += "org.json" % "json" % "20190722",
    libraryDependencies += "log4j" % "log4j" % "1.2.17",
    libraryDependencies += "org.slf4j" % "slf4j-log4j12" % "1.7.29",
    libraryDependencies += "org.apache.samoa" % "samoa" % samoaVersion,
    libraryDependencies += "org.apache.samoa" % "samoa-instances" % samoaVersion,
    libraryDependencies += "org.apache.samoa" % "samoa-api" % samoaVersion,
  )

assembly / mainClass := Some("br.ufscar.dc.ppgcc.gsdr.mfog.ModelStore")

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
