ThisBuild / resolvers ++= Seq(
    "Apache Development Snapshot Repository" at "https://repository.apache.org/content/repositories/snapshots/",
    Resolver.mavenLocal
)

name := "Mfog"
version := "0.4"
organization := "br.ufscar.dc.gsdr"

ThisBuild / scalaVersion := "2.11.12"
val flinkVersion = "1.10.0"
val javaVersion = "1.8"
val junitVersion = "4.13"
val junitJupiterVersion = "5.6.2"
val junitVintageVersion = "5.6.2"

val flinkDependencies = Seq(
  "org.apache.flink" %% "flink-scala" % flinkVersion % "provided",
  "org.apache.flink" %% "flink-streaming-scala" % flinkVersion % "provided",
  "org.apache.flink" %% "flink-java" % flinkVersion % "provided",
  "org.apache.flink" %% "flink-streaming-java" % flinkVersion % "provided",
  //
  "org.json" %% "json" % "20190722",
  
)

lazy val root = (project in file(".")).
  settings(
    libraryDependencies ++= flinkDependencies
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
