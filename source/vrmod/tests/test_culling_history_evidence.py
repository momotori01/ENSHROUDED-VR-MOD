"""Regression checks for per-frame proof, including asymmetric native warmup."""
import copy
import pathlib
import sys
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from check_culling_history import check_history


class EvidenceTests(unittest.TestCase):
    def fixture(self):
        return [dict(event='calibration_pair_recorded',serial=101),
                dict(event='live_native_pair_complete',serial=101),
                dict(event='culling_history_eye',serial=100,eye=0,substitutions=1),
                dict(event='culling_history_eye',serial=100,eye=1,substitutions=0),
                dict(event='culling_history_eye',serial=101,eye=0,substitutions=1),
                dict(event='culling_history_eye',serial=101,eye=1,substitutions=1),
                dict(event='culling_history_released',eye=0,substitutions=2,reset_commands=1),
                dict(event='culling_history_released',eye=1,substitutions=1,reset_commands=1)]

    def test_warmup_may_be_asymmetric(self):
        result=check_history(self.fixture(),True)
        self.assertTrue(result['culling_history_both_eyes_substituted'])
        self.assertTrue(result['culling_history_lifetime_calls_accounted'])

    def test_missing_submitted_eye_is_not_hidden_by_warmup(self):
        rows=self.fixture();rows[5]['substitutions']=0
        self.assertFalse(check_history(rows,True)['culling_history_both_eyes_substituted'])

    def test_duplicate_row_does_not_prove_two_eyes(self):
        rows=self.fixture();rows[5]=copy.copy(rows[4])
        self.assertFalse(check_history(rows,True)['culling_history_both_eyes_substituted'])

    def test_unobserved_lifetime_call_fails(self):
        rows=self.fixture();rows[-1]['substitutions']+=1
        self.assertFalse(check_history(rows,True)['culling_history_lifetime_calls_accounted'])


class GeneratedCountEvidenceTests(unittest.TestCase):
    def fixture(self,owner):
        rows=[dict(event='calibration_pair_recorded',serial=200),
              dict(event='culling_history_hook_enabled',rva='0x7caa0')]
        for eye in ([-1,0,1] if owner==-1 else [0,1]):
            rows.append(dict(event='generated_instance_count_reset',serial=200,eye=eye,
                             owner_eye=owner,reset=eye==owner,source_unchanged=True,count_bytes=4))
        return rows

    def test_original_or_left_can_own_generation(self):
        for owner in [-1,0]:
            result=check_history(self.fixture(owner),True)
            self.assertTrue(result['generated_instance_count_shared_per_pair'])
            self.assertTrue(result['generated_instance_count_borrower_has_proof'])

    def test_second_eye_clear_is_rejected(self):
        rows=self.fixture(0);rows[-1]['reset']=True
        self.assertFalse(check_history(rows,True)['generated_instance_count_shared_per_pair'])
        self.assertFalse(check_history(rows,True)['generated_instance_count_borrower_has_proof'])

    def test_previous_frame_proof_cannot_cover_current(self):
        rows=self.fixture(0);rows[2]['serial']=199
        self.assertFalse(check_history(rows,True)['generated_instance_count_shared_per_pair'])
        self.assertFalse(check_history(rows,True)['generated_instance_count_borrower_has_proof'])

    def test_wrong_owner_or_source_rejected(self):
        for key,value in [('owner_eye',-1),('source_unchanged',False),('count_bytes',8)]:
            rows=self.fixture(0);rows[-1][key]=value
            self.assertFalse(check_history(rows,True)['generated_instance_count_shared_per_pair'])

    def test_missing_or_duplicate_eye_rejected(self):
        for kind in ['missing','duplicate']:
            rows=self.fixture(-1)
            if kind=='missing':rows.pop()
            else:rows.append(copy.copy(rows[-1]))
            self.assertFalse(check_history(rows,True)['generated_instance_count_shared_per_pair'])

    def test_capped_samples_need_complete_lifetime_audit(self):
        rows=[dict(event='culling_history_hook_enabled',rva='0x7caa0')]
        for serial in range(1100):
            rows.append(dict(event='calibration_pair_recorded',serial=serial))
            if serial<1000:
                for eye in [-1,0,1]:
                    rows.append(dict(event='generated_instance_count_reset',serial=serial,eye=eye,
                                     owner_eye=-1,reset=eye==-1,source_unchanged=True,count_bytes=4))
        summary=dict(event='generated_instance_count_restored',resets=1100,borrows=2200,
                     audited_eyes=2200,calls=[1100,1100,1100],source_unchanged=True,failed=False)
        rows.append(summary)
        result=check_history(rows,True)
        self.assertTrue(result['generated_instance_count_shared_per_pair'])
        self.assertTrue(result['generated_instance_count_lifetime_audited'])
        summary['audited_eyes']=2199
        self.assertFalse(check_history(rows,True)['generated_instance_count_lifetime_audited'])
        summary['audited_eyes']=2200;summary['failed']=True
        self.assertFalse(check_history(rows,True)['generated_instance_count_lifetime_audited'])

    def test_missing_count_events_is_failure_not_checker_exception(self):
        rows=self.fixture(0)[:2]
        self.assertFalse(check_history(rows,True)['generated_instance_count_shared_per_pair'])


if __name__=='__main__':unittest.main()
