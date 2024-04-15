class Platform:

    def __init__(self):
        self.thread_affinity_mappings = {}

    def add_thread_affinity_mapping(self, thread, core_affinity):
        if thread not in self.thread_affinity_mappings:
            self.thread_affinity_mappings[thread] = core_affinity
        else:
            raise RuntimeError("Can't add tread mapping twice!")

    def get_affinities_of_threads(self, threads):
        return [self.thread_affinity_mappings[thread] for thread in threads]
