#pragma once

#include "../../common/http/http_client.hpp"

#include <memory>
#include <optional>
#include <string>

namespace apex::gateway {

/**
 * Cliente do motor de domínio em Haskell (`apex-strategy-hs`).
 *
 * O gateway envia evidência já medida e recebe de volta a narrativa explicável
 * e a análise de degradação. Se o motor estiver indisponível, o chamador recorre
 * ao resumo determinístico produzido em C++ — com menos contexto de domínio, mas
 * derivado exatamente dos mesmos números.
 */
class StrategyClient {
public:
    explicit StrategyClient(std::string base_url = {});

    bool configured() const { return !base_url_.empty(); }
    bool healthy() const;
    const std::string& base_url() const { return base_url_; }
    const std::string& last_error() const { return last_error_; }

    /** Devolve o array JSON `insights`, ou `nullopt` se o motor não respondeu. */
    std::optional<std::string> insights(const std::string& evidence_json);

    /** Devolve o array JSON `stints` analisados, ou `nullopt`. */
    std::optional<std::string> degradation(const std::string& stints_json);

private:
    std::string base_url_;
    std::unique_ptr<apex::common::HttpClient> http_;
    mutable std::string last_error_;

    std::optional<std::string> call(const std::string& path, const std::string& body,
                                    const std::string& array_field);
};

} // namespace apex::gateway
