import unittest

import numpy as np

from acoustic_prosody_experiment import acoustic_map


class AcousticMapTests(unittest.TestCase):
    def test_repeated_source_frames_have_one_mean_reference_time(self):
        points = acoustic_map([[.8, .1], [.9, .1], [1., .2], [1.2, .3]])
        np.testing.assert_allclose(points / 24000, [[.85, .1], [1., .2], [1.2, .3]])

    def test_failed_or_nonmonotone_alignment_cannot_be_used_as_pitch_control(self):
        for points in ([], [[0, 0]], [[0, 0], [1, -1]], [[0, 0], [float('nan'), 1]], [[0, 0], [0, 1]]):
            with self.assertRaises(ValueError):
                acoustic_map(points)
