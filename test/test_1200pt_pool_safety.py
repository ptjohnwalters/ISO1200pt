import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
POOL_XML = REPOSITORY_ROOT / "object_pool"
POOL_IOP = REPOSITORY_ROOT / "examples/1200PT/src/object_pool/object_pool.iop"
MAIN_CPP = (REPOSITORY_ROOT / "examples/1200PT/src/main.cpp").read_text()
FOLD_CPP = (REPOSITORY_ROOT / "examples/1200PT/src/fold_sequence.cpp").read_text()


class PoolSafetyTests(unittest.TestCase):
    def test_pool_version_is_derived_from_embedded_bytes(self):
        self.assertIn(
            "hash_object_pool_to_version(\n"
            "        object_pool_start,\n"
            "        objectPoolSize",
            MAIN_CPP,
        )
        self.assertRegex(
            MAIN_CPP,
            r"set_object_pool\(\s*0,\s*object_pool_start,\s*"
            r"static_cast<std::uint32_t>\(objectPoolSize\),\s*objectPoolVersion\s*\)",
        )
        self.assertIn(
            '"Embedded VT object pool: version=%s, size=%u bytes"',
            MAIN_CPP,
        )
        self.assertNotRegex(MAIN_CPP, r'set_object_pool\([\s\S]*?"1200"')

    def test_iop_has_six_individual_actions_per_direction(self):
        root = ET.parse(POOL_XML).getroot()
        names = {element.get("name") for element in root.iter()}
        expected = {
            f"Btn_{direction}Action{index}"
            for direction in ("Fold", "Unfold")
            for index in range(1, 7)
        }
        self.assertTrue(expected.issubset(names))
        self.assertEqual(POOL_IOP.read_bytes().count(b"ACTIVATE"), 12)

    def test_each_action_clears_outputs_before_energizing(self):
        for direction in ("fold", "unfold"):
            match = re.search(
                rf"void fold_sequence_activate_{direction}_action\([^)]*\)\s*\{{(.*?)\n\}}",
                FOLD_CPP,
                re.DOTALL,
            )
            self.assertIsNotNone(match)
            action_body = match.group(1)
            self.assertLess(
                action_body.index("fold_sequence_all_off();"),
                action_body.index("apply_solenoid_states("),
            )

    def test_fault_or_startup_inhibits_and_cancels_hydraulic_actions(self):
        self.assertEqual(MAIN_CPP.count("if (hydraulicActuationInhibited.load()) return;"), 2)
        self.assertIn(
            "hydraulicActuationInhibited.exchange(!hydraulicControlAvailable)",
            MAIN_CPP,
        )
        self.assertIn(
            "if (!hydraulicControlAvailable && !wasActuationInhibited)\n"
            "        {\n"
            "            fold_sequence_cancel();",
            MAIN_CPP,
        )
        self.assertIn("fold_sequence_all_off();", FOLD_CPP)

    def test_periodic_control_does_not_run_on_timer_service_task(self):
        self.assertNotIn("xTimerCreate", MAIN_CPP)
        self.assertIn("fan_vac_update();\n", MAIN_CPP)
        self.assertIn("update_display_values();\n", MAIN_CPP)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(PID_UPDATE_INTERVAL));", MAIN_CPP)


if __name__ == "__main__":
    unittest.main()
