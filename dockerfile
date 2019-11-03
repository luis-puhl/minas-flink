###############################################################################
# Build Click Count Job
###############################################################################

FROM maven:3.6-jdk-8-slim AS builder

# Get Click Count job and compile it
COPY ./java/flink-playground-clickcountjob /opt/flink-playground-clickcountjob
WORKDIR /opt/flink-playground-clickcountjob
RUN mvn clean install


###############################################################################
# Build Operations Playground Image
###############################################################################

FROM flink:1.9.0-scala_2.11

WORKDIR /opt/flink/bin

# Copy Click Count Job
COPY --from=builder /opt/flink-playground-clickcountjob/target/flink-playground-clickcountjob-*.jar /opt/ClickCountJob.jar
