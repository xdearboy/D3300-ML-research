import os,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import d3300_state_route_analyzer as routes
FW=Path(os.environ.get("D3300_FIRMWARE",""))

@unittest.skipUnless(FW.is_file(),"D3300_FIRMWARE not available")
class RouteTests(unittest.TestCase):
    def test_005a_fourteen_states_reduce_to_two_routes(self):
        result=routes.analyze(FW.read_bytes())
        self.assertTrue(routes.verify(result))
        self.assertEqual(result["route_count"],2)

if __name__=="__main__":unittest.main()
