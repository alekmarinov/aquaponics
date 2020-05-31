FROM python:3.7

ENV ESP32_WROOM_32_CSV=/data/esp32-wroom-32.csv

WORKDIR /app

COPY . .

RUN pip install -r requirements.txt

CMD python app.py
