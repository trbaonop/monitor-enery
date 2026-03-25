from flask import Flask, render_template
import gspread
from oauth2client.service_account import ServiceAccountCredentials
import pandas as pd
import json

app = Flask(__name__)

scope = [
'https://spreadsheets.google.com/feeds',
'https://www.googleapis.com/auth/drive'
]

creds = ServiceAccountCredentials.from_json_keyfile_name(
'carbon.json', scope)

client = gspread.authorize(creds)

sheet = client.open("Đồ An chuyên ngành").sheet1


@app.route("/")
def dashboard():

    data = sheet.get_all_records()
    df = pd.DataFrame(data)

    latest = df.iloc[-1]

    voltage = latest["Voltage"]
    current = latest["Current"]
    power = latest["Power"]
    energy = latest["Energy"]

    power_list = df["Power"].tail(20).tolist()
    time_list = df["Time"].tail(20).tolist()

    return render_template(
        "dashboard.html",
        voltage=voltage,
        current=current,
        power=power,
        energy=energy,
        power_list=json.dumps(power_list),
        time_list=json.dumps(time_list)
    )


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)