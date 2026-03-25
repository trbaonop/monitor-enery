import gspread
import os
import pandas as pd
from oauth2client.service_account import ServiceAccountCredentials
from sklearn.ensemble import IsolationForest

# ==============================
# KẾT NỐI GOOGLE SHEET
# ==============================

os.environ["HTTP_PROXY"] = ""
os.environ["HTTPS_PROXY"] = ""

scope = [
    'https://spreadsheets.google.com/feeds',
    'https://www.googleapis.com/auth/drive'
]

creds = ServiceAccountCredentials.from_json_keyfile_name(
    'carbon.json', scope)

client = gspread.authorize(creds)

sheet = client.open("Đồ An chuyên ngành").sheet1


# ==============================
# PHÁT HIỆN BẤT THƯỜNG
# ==============================

def detect_anomaly(df):

    power = df["Power"].values.reshape(-1, 1)

    model = IsolationForest(contamination=0.1)

    model.fit(power)

    result = model.predict(power)

    if result[-1] == -1:
        return "⚠ Bất thường - có thể quá tải"
    else:
        return "✅ Hệ thống bình thường"


# ==============================
# LẤY DỮ LIỆU
# ==============================

def get_data():

    data = sheet.get_all_records()

    df = pd.DataFrame(data)

    latest = df.iloc[-1]

    voltage = latest["Voltage"]
    current = latest["Current"]
    power = latest["Power"]
    energy = latest["Energy"]

    status = detect_anomaly(df)

    return voltage, current, power, energy, status


# ==============================
# TRỢ LÝ ẢO
# ==============================

def assistant():

    print("🤖 Energy AI Assistant")
    print("Bạn có thể hỏi:")
    print("voltage | current | power | energy | status | exit")

    while True:

        question = input("\nBạn: ").lower()

        voltage, current, power, energy, status = get_data()

        if "voltage" in question:
            print(f"AI: Điện áp hiện tại là {voltage} V")

        elif "current" in question:
            print(f"AI: Dòng điện hiện tại là {current} A")

        elif "power" in question:
            print(f"AI: Công suất hiện tại là {power} W")

        elif "energy" in question:
            print(f"AI: Tổng điện năng tiêu thụ {energy} kWh")

        elif "status" in question:
            print(f"AI: {status}")

        elif "exit" in question:
            print("AI: Tạm biệt!")
            break

        else:
            print("AI: Tôi có thể cung cấp thông tin Voltage, Current, Power, Energy hoặc Status")


# ==============================
# CHẠY TRỢ LÝ
# ==============================

assistant()