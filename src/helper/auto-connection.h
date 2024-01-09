// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SEEN_AUTO_CONNECTION_H
#define SEEN_AUTO_CONNECTION_H

#include <sigc++/connection.h>

namespace Inkscape {

// Class to simplify re-subscribing to connections; automates disconnecting

class auto_connection
{
public:
    auto_connection(sigc::connection const &c)
        : _connection(c)
    {}

    auto_connection() = default;

    ~auto_connection() { _connection.disconnect(); }

    auto_connection(auto_connection const &) = delete;
    auto_connection &operator=(auto_connection const &) = delete;

    // re-assign
    auto_connection &operator=(sigc::connection const &c)
    {
        _connection.disconnect();
        _connection = c;
        return *this;
    }

    /** Returns whether the connection is still active
     *  @returns @p true if connection is still ative
     */
    operator bool() const noexcept { return _connection.connected(); }

    /** Returns whether the connection is still active
     *  @returns @p true if connection is still ative
     */
    inline bool connected() const noexcept { return _connection.connected(); }

    /** Sets or unsets the blocking state of this connection.
     * @param should_block Indicates whether the blocking state should be set or unset.
     * @return @p true if the connection has been in blocking state before.
     */
    inline bool block(bool should_block = true) noexcept
    {
        return _connection.block(should_block);
    }

    inline bool unblock() noexcept { return _connection.unblock(); }

    void disconnect() { _connection.disconnect(); }

private:
    sigc::connection _connection;
};

} // namespace Inkscape

#endif // SEEN_AUTO_CONNECTION_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :

