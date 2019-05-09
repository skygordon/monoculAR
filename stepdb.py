
import datetime
import pandas as pd
import sqlite3

visits_db = '__HOME__/step.db'
step_db = 'step.db' # name of database
def create_database():
    conn = sqlite3.connect(visits_db)  # connect to that database (will create if it doesn't already exist)
    c = conn.cursor()  # make cursor into database (allows us to execute commands)
    c.execute('''CREATE TABLE IF NOT EXISTS step_table (steps int, day int, month int);''') # run a CREATE TABLE command
    conn.commit() # commit commands
    conn.close() # close connection to database
 
create_database()  #call the function!

def insertstart():
    conn = sqlite3.connect(visits_db)
    c = conn.cursor()
    c.execute('''INSERT into step_table VALUES (?,?,?);''', (0 , pd.datetime.now().day, pd.datetime.now().month))
    conn.commit()
    conn.close()

insertstart()

#
#def findstepcount():
#    conn = sqlite3.connect(step_db)
#    c = conn.cursor()
#    c.execute('''SELECT steps FROM step_table WHERE day = ? AND month = ? ORDER BY steps DESC;''', (reqday, reqmonth))
#    laststeps = c.fetchone()
#    conn.commit()
#    conn.close()
#    return laststeps
#
#
#def findstepday():
#    conn = sqlite3.connect(step_db)
#    c = conn.cursor()
#    c.execute('''SELECT day FROM step_table WHERE day = ? AND month = ? ORDER BY steps DESC;''', (reqday, reqmonth))
#    lastday = c.fetchone()
#    conn.commit()
#    conn.close()
#    return lastday
#
#print(findstepcount()[0])

def request_handler(request):

    def findstepcount():
        conn = sqlite3.connect(visits_db)
        c = conn.cursor()
        c.execute('''SELECT steps FROM step_table WHERE day = ? AND month = ? ORDER BY steps DESC;''', (reqday, reqmonth))
        laststeps = c.fetchone()
        conn.commit()
        conn.close()
        return laststeps

    def findstepday():
        conn = sqlite3.connect(visits_db)
        c = conn.cursor()
        c.execute('''SELECT day FROM step_table WHERE day = ? AND month = ? ORDER BY steps DESC;''', (reqday, reqmonth))
        lastday = c.fetchone()
        conn.commit()
        conn.close()
        return lastday
    

    if (request['method']=="POST"):
        step = int(request['form']['step']) #getting the current step-count value
        
        conn = sqlite3.connect(visits_db)
        c = conn.cursor()
        c.execute('''INSERT into step_table VALUES (?,?,?);''', (step, pd.datetime.now().day, pd.datetime.now().month))
        conn.commit()
        conn.close()
        
        
    elif (request['method']=="GET"):
        reqday = int(request['values']['day']) # date requested
        reqmonth = pd.datetime.now().month # current month, i think it is already an int
        if reqday > pd.datetime.now().day:
            return 'Day {} does not exist yet'.format(reqday)
        
        else:
            if reqmonth == 1:
                pmonth = "January"
            elif reqmonth == 2:
                pmonth = "February"
            elif reqmonth == 3:
                pmonth = "March"
            elif reqmonth == 4:
                pmonth = "April"
            elif reqmonth == 5:
                pmonth = "May"
            elif reqmonth == 6:
                pmonth = "June"
            elif reqmonth == 7:
                pmonth = "July"
            elif reqmonth == 8:
                pmonth = "August"
            elif reqmonth == 9:
                pmonth = "September"    
            elif reqmonth == 10:
                pmonth = "October"
            elif reqmonth == 11:
                pmonth = "November"
            elif reqmonth == 12:
                pmonth = "December"
            else:
                pmonth = "the month"
            
            stepsday = int(findstepday()[0])
            totalstep = int(findstepcount()[0])

            return 'Steps of {} {}: {}'.format(stepsday, pmonth, totalstep)






"""

Created on Fri May  3 10:16:45 2019
@author: skylargordon
"""

