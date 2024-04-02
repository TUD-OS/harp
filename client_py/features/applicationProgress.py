from client_py.feature import Feature
from proto.tetris_pb2 import ClientResponse, ServerMessage


class ApplicationProgress(Feature):
    """
    Python class equivalent to the ApplicationProgress C++ class.
    """

    def __init__(self):
        super().__init__()
        self._logger = None
        self._progress = 0.0

    def need_handshake(self) -> bool:
        """
        Implements the need_handshake method.
        """
        return True

    def handshake(self) -> int:
        """
        Implements the handshake method.
        """
        # Return the FeatureID (assuming it's an integer)
        return 0

    def handle(self, msg: ServerMessage) -> ClientResponse:
        """
        Implements the handle method.

        :param msg: ServerMessage received.
        :return: ClientResponse to be sent back.
        """
        # Implement the handle logic here
        pass

    def update_progress(self, progress: float):
        """
        Updates the application progress and logs a debug message.

        :param progress: The progress to set.
        """
        self._progress = progress
        if self._logger:
            self._logger.debug(f"Updated application progress to {progress}")
        else:
            # Assuming debug method prints to console if logger is None
            print(f"Updated application progress to {progress}")
