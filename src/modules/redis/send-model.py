import redis
import sys
import csv
import time

#define MODEL_STORE_UNKNOWNS_LIST "minas-unkowns"
#define MODEL_STORE_MODEL_LIST "minas-clusters"
#define MODEL_STORE_MODEL_UPDATE_CHANNEL "model-up-ch"

if __name__ == "__main__":
    # main(args)
    cli = redis.Redis.from_url('redis://localhost:6379')
    # cli = redis.Redis.from_url('redis://ec2-18-191-2-174.us-east-2.compute.amazonaws.com:6379')
    # cli.execute_command("HELLO 3")
    # print('ping', cli.ping())
    # broker = cli.pubsub()

    with cli.pipeline() as pipe:
        data = sys.stdin.readlines()
        for line in csv.reader(data):
            # for line in data:
            if line[0].startswith('#'):
                continue
            id = int(line[0])
            fullline = ','.join(line)
            cli.rpush('minas-clusters', fullline)
            # print(line[1])
        pipe.execute()
    cli.publish('model-up-ch', time.asctime())

    cli.close()
