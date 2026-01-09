project(monitoring)

install(
    DIRECTORY
        clickhouse
        grafana 
        postgres
    DESTINATION
        ${CMAKE_INSTALL_PREFIX}/memhawk/monitoring
)
install(
    FILES
        docker-compose.yaml
        .env
        README.md
    DESTINATION
        ${CMAKE_INSTALL_PREFIX}/memhawk/monitoring
)
