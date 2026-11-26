import os,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import d3300_info_state_gates as gates
FW=Path(os.environ.get("D3300_FIRMWARE",""))

@unittest.skipUnless(FW.is_file(),"D3300_FIRMWARE not available")
class StateGateTests(unittest.TestCase):
    def test_005a_005c_are_bounded_but_unnamed(self):
        result=gates.analyze(FW.read_bytes())
        self.assertTrue(gates.verify(result))
        self.assertIsNone(result["terminal_semantic_evidence"])

if __name__=="__main__": unittest.main()
