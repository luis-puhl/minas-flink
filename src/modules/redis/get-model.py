import redis
import sys
import csv

if __name__ == "__main__":
    # main(args)
    cli = redis.Redis.from_url('redis://ec2-18-191-2-174.us-east-2.compute.amazonaws.com:6379')
    print('ping', cli.ping())
    broker = cli.pubsub()

    def my_handler(message):
        print('handle: ', message['data'])
    broker.subscribe(**{'model': my_handler})
    broker.subscribe('model')
    msg = broker.get_message()
    if msg:
        print('get_message: ', msg['data'])
    length = cli.llen('model')
    model = cli.lrange('model', 0, length)
    for i in model:
        print('range', i)

    cli.close()
