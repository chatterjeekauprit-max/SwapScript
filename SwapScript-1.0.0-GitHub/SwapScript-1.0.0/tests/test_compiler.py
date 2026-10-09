import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from compiler import compile_source, fnv1a


SAMPLE = """\
context daylight params HEAD_DAYLIGHT
context night params HEAD_NIGHT
initial daylight
transition daylight -> night when ambient_lux <= 20 hold 50 ms
transition night -> daylight when ambient_lux >= 30 hold 100 ms
"""


class CompilerTests(unittest.TestCase):
    def test_fnv1a_known_value(self):
        self.assertEqual(fnv1a("hello"), 0x4F9F2CAB)

    def test_emits_c99_rule_table(self):
        output = compile_source(SAMPLE)
        self.assertIn("#define SS_CONTEXT_DAYLIGHT 0x30da2927u", output)
        self.assertIn("#define SS_METRIC_AMBIENT_LUX 0x9601af05u", output)
        self.assertIn("20.0f, 50u, SS_LE", output)
        self.assertIn("30.0f, 100u, SS_GE", output)
        self.assertIn("#define SS_GENERATED_RULE_COUNT 2u", output)

    def test_rejects_invalid_references(self):
        with self.assertRaisesRegex(ValueError, "invalid transition"):
            compile_source("context a params P\ninitial a\ntransition a -> b when m > 1 hold 2 ms")

    def test_rejects_macro_name_collision(self):
        source = "context foo params P\ncontext FOO params Q\ninitial foo\ntransition foo -> FOO when m > 1 hold 2 ms"
        with self.assertRaisesRegex(ValueError, "uppercasing"):
            compile_source(source)

    def test_rejects_excessive_hold(self):
        with self.assertRaisesRegex(ValueError, r"less than 2\^31"):
            compile_source("context a params P\ncontext b params Q\ninitial a\ntransition a -> b when m > 1 hold 2147483648 ms")

    def test_requires_transition(self):
        with self.assertRaisesRegex(ValueError, "at least one transition"):
            compile_source("context a params P\ninitial a")

    def test_rejects_non_c_metric_identifier(self):
        with self.assertRaisesRegex(ValueError, "invalid metric identifier"):
            compile_source("context a params P\ncontext b params Q\ninitial a\ntransition a -> b when métrica > 1 hold 2 ms")

    def test_rejects_non_finite_c_float(self):
        with self.assertRaisesRegex(ValueError, "finite C float"):
            compile_source("context a params P\ncontext b params Q\ninitial a\ntransition a -> b when m > 1e999 hold 2 ms")

    def test_accepts_comments_and_exponent_threshold(self):
        output = compile_source("# header\ncontext a params P\ncontext b params Q\ninitial a\ntransition a -> b when metric > 1.25e1 hold 0 ms # immediate\n")
        self.assertIn("12.5f, 0u, SS_GT", output)

    def test_enforces_configured_capacities(self):
        with self.assertRaisesRegex(ValueError, "context count exceeds capacity"):
            compile_source(SAMPLE, max_contexts=1)
        with self.assertRaisesRegex(ValueError, "metric count exceeds capacity"):
            compile_source("context a params P\ncontext b params Q\ninitial a\ntransition a -> b when first > 0 hold 0 ms\ntransition a -> b when second > 0 hold 0 ms", max_metrics=1)


if __name__ == "__main__":
    unittest.main()
