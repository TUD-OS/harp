from abc import ABC, abstractmethod
from proto.tetris_pb2 import ServerMessage, ClientResponse


class Feature(ABC):
    """
    TETRiS Feature abstract class.

    A TETRiS feature is a user of the TETRiS system. A feature is bound to the TETRiS client which runs on
    the application side. This binding procedure allows the feature to communicate with the TETRiS server and to
    receive push notifications if needed.
    """

    def __init__(self):
        self._client = None  # Bounded TETRiS client instance

    @abstractmethod
    def handle(self, msg: ServerMessage) -> ClientResponse:
        """
        Handle a message from the TETRiS server for this feature.

        Called from the push listener thread when a command is received.

        :param msg: ServerMessage received.
        :return: ClientResponse associated to the message.
        """
        pass

    @abstractmethod
    def need_handshake(self) -> bool:
        """
        Checks if the feature needs a handshake.

        A handshake is only needed in order to subscribe to push notifications from the server.

        :return: True if the feature needs a handshake, False otherwise.
        """
        pass

    @abstractmethod
    def handshake(self) -> int:
        """
        Performs a handshake with the TETRiS server.

        :return: Allocated feature ID.
        """
        pass

    def accept(self, client):
        """
        Accepts the TETRiS client to bind the feature.

        :param client: Pointer to the client.
        """
        self._client = client

    def is_bounded(self) -> bool:
        """
        Checks if the feature is bounded to a TETRiS client.

        This method can be used to check if the feature is connected.

        :return: True if bounded, False otherwise.
        """
        return self._client is not None

    def get_client(self):
        """
        Gets the TETRiS client instance.

        :return: Pointer to the TETRiS client.
        """
        return self._client
