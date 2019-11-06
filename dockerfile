FROM flink

# RUN curl https://bintray.com/sbt/rpm/rpm > /etc/yum.repos.d/bintray-sbt-rpm.repo
# RUN yum install sbt --assumeyes
WORKDIR /
RUN echo "deb https://dl.bintray.com/sbt/debian /" >> /etc/apt/sources.list.d/sbt.list && \
  curl -sL "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x2EE0EA64E40A89B84B2DF73499E82A75642AC823" | apt-key add && \
  apt-get update ; \
  apt-get install apt-transport-https -y && \
  apt-get update && \
  apt-get install sbt -y
RUN echo "sbt" | sbt new scala/scala-seed.g8
WORKDIR /sbt
RUN sbt compile

COPY ./flink-project /code
WORKDIR /code
RUN sbt assembly
# RUN mv /code/target/*.jar /opt/
# RUN mvn clean install

# FROM flink:1.9.0-scala_2.11

WORKDIR /opt/flink/bin

# Copy Click Count Job
# COPY --from=sbt /opt/flink-playground-clickcountjob/target/flink-playground-clickcountjob-*.jar /opt/ClickCountJob.jar
