import redis
import sys
import csv

if __name__ == "__main__":
    # cli = redis.Redis.from_url('redis://localhost:6379')
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
    for i in cli.lrange('model', 0, -1):
        print('range', i)

    cli.close()
