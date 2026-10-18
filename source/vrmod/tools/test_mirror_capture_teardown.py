"""Regression for startup VkInstance destruction falsely passing game cleanup."""
import unittest
from check_mirror_capture import normal_game_teardown


def fixture():
    return [
        # The real loader can recycle this handle for the game instance.
        dict(us=1, event='create_instance', instance='parent'),
        dict(us=2, event='destroy_instance', instance='parent'),
        dict(us=3, event='create_instance', instance='parent'),
        dict(us=4, event='create_device', instance='parent', device='game'),
        dict(us=5, event='game_capture'),
        dict(us=6, event='game_frame', result=0),
        dict(us=7, event='destroy_device', device='game'),
        dict(us=8, event='destroy_instance', instance='parent'),
    ]


class TeardownTests(unittest.TestCase):
    def test_completed_game_after_reused_startup_handle(self):
        self.assertTrue(normal_game_teardown(fixture()))

    def test_only_startup_instance_ended(self):
        self.assertFalse(normal_game_teardown(fixture()[:6]))

    def test_unrelated_instance_end_cannot_finish_game(self):
        events = fixture()
        events[-1]['instance'] = 'unrelated'
        self.assertFalse(normal_game_teardown(events))

    def test_wrong_device_end(self):
        events = fixture()
        events[-2]['device'] = 'unrelated'
        self.assertFalse(normal_game_teardown(events))

    def test_parent_destroyed_before_device(self):
        events = fixture()
        events[-1]['us'] = 6.5
        self.assertFalse(normal_game_teardown(events))

    def test_capture_after_device_end(self):
        events = fixture()
        events[4]['us'] = 7.5
        self.assertFalse(normal_game_teardown(events))

    def test_parent_already_destroyed_before_device_creation(self):
        events = fixture()
        del events[2]
        self.assertFalse(normal_game_teardown(events))

    def test_multiple_devices_without_capture_device_ids_are_ambiguous(self):
        events = fixture()
        events.insert(4, dict(us=4.5, event='create_device', instance='parent', device='other'))
        self.assertFalse(normal_game_teardown(events))


if __name__ == '__main__':
    unittest.main(verbosity=2)
