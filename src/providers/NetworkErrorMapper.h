#ifndef NETWORK_ERROR_MAPPER_H
#define NETWORK_ERROR_MAPPER_H

#include <QNetworkReply>
#include <QString>

// Shared mapping from QNetworkReply error codes to the fixed, translatable
// English messages used by both the translation pipeline and model detection.
// Server payload messages (a JSON `error.message`, for instance) always win
// over these generic texts — callers keep that precedence themselves.
namespace NetworkErrorMapper {

// Maps a QNetworkReply error code onto a user-facing message. Unknown codes
// collapse onto the generic "Network error".
QString messageFor(QNetworkReply::NetworkError error);

}  // namespace NetworkErrorMapper

#endif  // NETWORK_ERROR_MAPPER_H
