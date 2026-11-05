"""Send/list admin Wi-Fi commands; reads the ignored local token file."""
import argparse
import json
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'web'))
from device_admin import remote_wifi_request,COMMANDS,AdminUnavailable

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('command',nargs='?',choices=COMMANDS,help='Omit to list recent commands')
args=parser.parse_args()
try:
    code,body=remote_wifi_request(args.command)
    print(json.dumps({'httpStatus':code,**body},indent=2))
    sys.exit(0 if code<400 else 1)
except AdminUnavailable as error:parser.exit(1,str(error)+'\n')
