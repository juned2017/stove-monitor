FROM python:3.12-slim
RUN pip install --no-cache-dir paho-mqtt==2.1.0
WORKDIR /app
COPY fake_esp32.py .
ENV PYTHONUNBUFFERED=1
ENTRYPOINT ["python", "fake_esp32.py"]
CMD ["cook"]
