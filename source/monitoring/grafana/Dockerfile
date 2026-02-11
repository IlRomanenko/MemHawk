FROM grafana/grafana:12.2

ARG HTTP_PROXY
ARG HTTPS_PROXY

RUN grafana-cli plugins install grafana-clickhouse-datasource && \
    grafana-cli plugins install williamvenner-timepickerbuttons-panel && \
    grafana-cli plugins install volkovlabs-variable-panel
