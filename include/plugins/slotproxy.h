// Copyright (c) 2023 UltiMaker
// CuraEngine is released under the terms of the AGPLv3 or higher

#ifndef PLUGINS_SLOTPROXY_H
#define PLUGINS_SLOTPROXY_H

#include <concepts>
#include <functional>
#include <memory>
#include <optional>

#include <grpcpp/channel.h>

#include "plugins/converters.h"
#include "plugins/pluginproxy.h"
#include "plugins/types.h"
#include "plugins/validator.h"

namespace cura::plugins
{

/**
 * @brief A class template representing a proxy for a plugin slot.
 *
 * The SlotProxy class template acts as a proxy for a plugin slot and provides an interface
 * for communication with plugins assigned to the slot. It delegates plugin requests to the
 * corresponding PluginProxy object and provides a default behavior when no plugin is available.
 *
 * @tparam Slot The plugin slot ID.
 * @tparam Validator The type used for validating the plugin.
 * @tparam Stub The process stub type.
 * @tparam Prepare The prepare type.
 * @tparam Request The gRPC convertible request type.
 * @tparam Response The gRPC convertible response type.
 * @tparam Default The default behavior when no plugin is available.
 */
template<plugins::SlotID Slot, std::convertible_to<bool> Validator, class Stub, class Prepare, grpc_convertable Request, grpc_convertable Response, auto Default>
class SlotProxy
{
    std::optional<PluginProxy<Slot, Validator, Stub, Prepare, Request, Response>> plugin_{ std::nullopt };

public:
    static inline constexpr plugins::SlotID slot_id{ Slot };

    /**
     * @brief Default constructor.
     *
     * Constructs a SlotProxy object without initializing the plugin.
     */
    constexpr SlotProxy() noexcept = default;

    /**
     * @brief Constructs a SlotProxy object with a plugin.
     *
     * Constructs a SlotProxy object and initializes the plugin using the provided gRPC channel.
     *
     * @param channel A shared pointer to the gRPC channel for communication with the plugin.
     */
    SlotProxy(std::shared_ptr<grpc::Channel> channel) : plugin_{ std::move(channel) } {};

    /**
     * @brief Executes the plugin operation.
     *
     * This operator allows the SlotProxy object to be invoked as a callable, which delegates the
     * plugin request to the corresponding PluginProxy object if available. If no plugin is available,
     * it invokes the default behavior provided by the `Default` callable object.
     *
     * @tparam Args The argument types for the plugin request.
     * @param args The arguments for the plugin request.
     * @return The result of the plugin request or the default behavior.
     */
    auto operator()(auto&&... args)
    {
        if (plugin_.has_value())
        {
            return std::invoke(plugin_.value(), std::forward<decltype(args)>(args)...);
        }
        return std::invoke(Default, std::forward<decltype(args)>(args)...);
    }
};

} // namespace cura::plugins


#endif // PLUGINS_SLOTPROXY_H
