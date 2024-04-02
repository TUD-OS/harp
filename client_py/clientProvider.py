from client import Client
from concreteClient import ConcreteClient


class ClientProvider:
    """
    TETRiS client singleton.

    Provides an instance of a ConcreteClient.
    """

    _instance = None

    @staticmethod
    def initialize(socket_path: str, platform_path: str, mapping_path: str):
        """
        Initializes a TETRiS client.

        :param socket_path: Path of the TETRiS server socket.
        :param platform_path: Path of the platform.
        :param mapping_path: Path of the mapping.
        """
        if ClientProvider._instance is None:
            ClientProvider._instance = ConcreteClient(socket_path, platform_path, mapping_path)

    @staticmethod
    def finalize():
        """
        Finalizes the TETRiS client.
        """
        if ClientProvider._instance is not None:
            ClientProvider._instance = None

    @staticmethod
    def get_instance() -> Client:
        """
        Gets the TETRiS client instance.

        :return: Pointer to the TETRiS client instance.
        """
        return ClientProvider._instance
