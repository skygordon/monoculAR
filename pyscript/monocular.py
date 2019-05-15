import requests
import math

def request_handler(request):
    if request['method'] == 'GET':
        values = request['values']
        if 'type' not in values:
            return 'Please specify type'
        elif values['type'] == 'shuttle' or values['type'] == 'yelp':
            try:
                lat = float(request['values']['lat'])
                lon = float(request['values']['lon'])
                if values['type'] == 'shuttle':
                    return shuttle_req(lat, lon)
                else:
                    return yelp_req(lat, lon)
            except ValueError:
                return 'Both lat and lon must be numbers'
        else:
            return 'Unknown type'
    else:
        return 'Unknown request'

def shuttle_req(lat, lon):
    #Fetch shuttle info
    r = requests.get('http://m.mit.edu/apis/shuttles/routes/tech').json()
    stops = r['stops'] #Extract the stops
    best_i = -1
    best_d = 10000
    for i in range(len(stops)):
        plat, plon = stops[i]['lat'], stops[i]['lon']
        p_d = math.sqrt((plat-lat)**2 + (plon-lon)**2)
        if p_d < best_d and 'predictions' in stops[i]:
            best_d = p_d
            best_i = i
    if best_i == -1: #No shuttles running
        return '0\nERROR: No shuttles found'
    #Extract times from our closest stop
    pred = stops[best_i]['predictions']
    times = [round(vehicle['seconds']/60, 1) for vehicle in pred]
    times.sort() #we want the soonest shuttles
    #add 'min' to each time
    return '{}\n{} min\n'.format(stops[best_i]['title'],
                       ' min\n'.join(list(map(str,
                        times[:3] if len(times) > 3 else times))))

def yelp_req(lat, lon):
    r = requests.get(f'https://api.yelp.com/v3/businesses/search',
                     {'term': 'food', 'latitude': lat, 'longitude': lon,
                      'radius': '2000', 'limit': '3', 'sort_by': 'distance',
                      'open_now': 'true'},
                     headers={'Authorization': 'Bearer sGN_D0OAwK5utfjVdZtq7gbNaT24Ki1_oHx7bRt6sMOI\
BiJUtuKB8LNawLk6SntgTv77sYq3x0Z2OIF4JPKMSntu75vsw8rwFcvb03-W3j03hDngMNS4FOKl_n\
7CXHYx'}).json() #gets the three closest restaurants
    return str(len(r['businesses'])) + '\n' \ #number of businesses found
           + '\n'.join(['{}\n{} stars\n{}\n{}\n{} meters'.format(
               x['name'], x['rating'], #name, rating
               x['categories'][0]['title'] if len(x['categories']) > 0
               else 'Food', #category
               x['location']['address1'] or 'No address', #address
               round(x['distance'])) #distance
              for x in r['businesses']]) #for each business
