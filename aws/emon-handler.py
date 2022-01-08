"""
AWS Lambda function to process messages from the Arduino home energy monitor.
"""
import boto3
import os
from decimal import Decimal

# Global declarations
db = boto3.resource('dynamodb')
table = db.Table(os.getenv('TABLE_NAME', 'test-sensor-data'))
ttl = os.getenv('TIME_TO_LIVE', 604800)

# Handler function
def handler(event, context):
    """
    This function separates and flattens messages from the Arduino home energy
    monitor and inserts them into a DynamoDB table.

        Parameters:
            event (dict) -- Data passed from the IoT Core service
            context (obj) -- AWS Lambda function context object

        Returns: None
    """
    with table.batch_writer() as batch:
        for msg in event:
            item = {
                'timestamp': msg['time'],
                'voltage': Decimal(str(msg['data']['voltage'])),
                'current': msg['data']['current'],
                'power': int(msg['data']['voltage'] * msg['data']['current']),
                'expdate': msg['time'] + ttl
            }

            batch.put_item(Item=item)
