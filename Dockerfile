FROM arm32v7/ubuntu

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y gcc python3 && pip3 install parse

COPY judger.py /root/.

COPY testcases/公开样例与运行时库/libsysy.a /root/.

CMD ["cd root && python3 judger.py"]

