import redis
import sys
import csv

if __name__ == "__main__":
    # main(args)
    cli = redis.Redis.from_url('redis://ec2-18-191-2-174.us-east-2.compute.amazonaws.com:6379')
    print('ping', cli.ping())
    broker = cli.pubsub()

    data = sys.stdin.readlines()
    for line in csv.reader(data):
    # for line in data:
        if line[0].startswith('#'):
            continue
        id = int(line[0])
        fullline = ','.join(line)
        cli.publish('model', fullline)
        cli.rpush('model', fullline)
        print(line[1])

    cli.close()
