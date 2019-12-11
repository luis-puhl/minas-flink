export PATH=/c/dev/minas-flink/mvn-shennenigans/bin:/c/dev/minas-flink/mvn-shennenigans/apache-maven-3.6.3/bin:$PATH
mvn clean install
java -cp "target/udemy-examples-1.0-SNAPSHOT.jar" examples.scala.KMeansVector.KMeansVector 
