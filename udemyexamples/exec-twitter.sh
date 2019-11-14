mvn clean package

../flink-1.9.1/bin/flink run \
    -c examples.Ex15Twitter target/udemy-examples-1.0-SNAPSHOT.jar \
    --twitter-source.consumerKey $TWITTER_API_KEY \
    --twitter-source.consumerSecret $TWITTER_API_KEY_SECRET \
    --twitter-source.token $TWITTER_ACCESS_TOKEN \
    --twitter-source.tokenSecret $TWITTER_ACCESS_TOKEN_SECRET #\
    #--output out
