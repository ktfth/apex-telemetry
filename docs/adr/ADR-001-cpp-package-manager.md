# ADR-001: Escolha do Gerenciador de Pacotes C++ (Conan 2 vs vcpkg)

## Status
Aceito

## Contexto
O ecossistema C++23 do ApexTelemetry compreende três serviços de backend (`api-gateway-cpp`, `ingest-cpp`, `analytics-cpp`) que exigem bibliotecas externas modernas:
- `drogon` (API REST / WebSocket)
- `boost` (Boost.Asio)
- `simdjson` (Parsing JSON de alta performance)
- `libcurl` (Transferência HTTP/REST)
- `libpqxx` (Driver PostgreSQL com suporte a transações modernas)
- `redis-plus-plus` (Cliente Redis com pooling e pipelines)
- `spdlog` e `fmt` (Logging estruturado de alto desempenho)
- `gtest` / `catch2` (Testes unitários)

A especificação exige a escolha fundamentada entre **Conan** e **vcpkg**.

## Decisão
Adotamos o **Conan 2** (`conan 2.32+`) como gerenciador de dependências C++ padrão do projeto.

### Justificativas:
1. **Integração com CMake**: Conan 2 gera arquivos `CMakeToolchain` e `CMakeDeps` padronizados, desacoplando o build de caminhos absolutos do sistema hospedeiro.
2. **Controle de Binários e Cache Local**: Conan 2 isola perfis de compilação (`Release`, `Debug`, `RelWithDebInfo`) e gerencia múltiplos compiladores (GCC 16, Clang 22) com detecção automática de ABI e C++23 sem conflitos na árvore de pacotes.
3. **Lockfiles Nativos**: Permite reprodutibilidade estrita em pipelines de CI/CD através de `conan.lock`.
4. **Disponibilidade no Ambiente**: Conan 2 está instalado e testado nativamente via ecossistema isolado de tooling do ambiente.

## Consequências
- Todo serviço C++ conterá um `conanfile.py` ou `conanfile.txt` declarando dependências e opções estritas.
- O build local utilizará `conan install . --build=missing -s build_type=Release` gerando pasta de presets CMake consumível por `cmake --preset conan-release`.
