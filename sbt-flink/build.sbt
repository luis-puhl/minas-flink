ThisBuild / resolvers ++= Seq(
    "Apache Development Snapshot Repository" at "https://repository.apache.org/content/repositories/snapshots/",
    Resolver.mavenLocal
)

name := "sbt-flink"
version := "0.1-SNAPSHOT"
organization := "org.example"

ThisBuild / scalaVersion := "2.11.12"

val flinkVersion = "1.9.1"

val flinkDependencies = Seq(
  "org.apache.flink" %% "flink-scala" % flinkVersion % "provided",
  "org.apache.flink" %% "flink-streaming-scala" % flinkVersion % "provided")

val loggerDependencies = Seq(
  "log4j" % "log4j" % "1.2.17",
  // "org.slf4j" %% "slf4j-api" % "1.7.5",
  // "org.slf4j" %% "slf4j-log4j12" % "1.7.7",
  "org.slf4j" % "slf4j-log4j12" % "1.7.29"
  )

lazy val root = (project in file(".")).
  settings(
    libraryDependencies ++= flinkDependencies,
    libraryDependencies ++= loggerDependencies
  )

assembly / mainClass := Some("org.example.Job")

// make run command include the provided dependencies
Compile / run  := Defaults.runTask(Compile / fullClasspath,
                                   Compile / run / mainClass,
                                   Compile / run / runner
                                  ).evaluated

// stays inside the sbt console when we press "ctrl-c" while a Flink programme executes with "run" or "runMain"
Compile / run / fork := true
Global / cancelable := true

// exclude Scala library from assembly
assembly / assemblyOption  := (assembly / assemblyOption).value.copy(includeScala = false)

fork in run := true
