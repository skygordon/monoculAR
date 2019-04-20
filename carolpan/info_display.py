"""
This contains the server-side script for the updating of the header information display
"""

import datetime
import requests
import json

# read the api from a designated text file
api_key = open('appid.txt').read()


def request_handler(request):

    # updating current day and time
    current_time = datetime.datetime.now()
    time = current_time.strftime("%H:%M")
    date = current_time.strftime("%m/%d/%Y")

    # get longitude and latitude for accurate weather data
    lon = request.get("form").get("lon")
    lat = request.get("form").get("lat")

    # making the api call
    r = requests.get("http://api.openweathermap.org/data/2.5/weather?lat=%s&lon="
                     "%s&appid=%s&units=imperial" % (lon, lat, api_key))
    response = json.loads(r.text)
    visibility = response["weather"][0]["main"]
    sunrise = response["sys"][]
    if visibility.lower() == "clear":
        if time
            visibility = "sun"
        else:
            visibility = "moon"
    else:
        visibility = "cloud"
