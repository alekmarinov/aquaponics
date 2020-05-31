import re
import requests
from datetime import datetime

def parse_value(content, pattern):
    return float(re.match(pattern, content.strip()).group(1))

def print_data(water_temperature=0, air_temperature=0, moisture=0, ph=0):
    print(",".join(map(str, [datetime.now().strftime('%Y-%m-%d %H:%M:%S'), water_temperature, air_temperature, moisture, ph])))

def main():
    # get html
    html = requests.get('http://192.168.1.110').text

    # parse values
    parsed_values = { k: parse_value(html, pattern) for k, pattern in dict(
        water_temperature=".*Water Temperature: (\d*\\.\d*).*",
        air_temperature=".*Air Temperature: (\d*\\.\d*)",
        moisture=".*Moisture: (\d*)%",
        ph=".*pH: (\d*\\.\d*)"
    ).items() }

    # insert data
    print_data(**parsed_values)
    
if __name__ == "__main__":
    main()
