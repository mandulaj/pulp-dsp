#!/usr/bin/env python3

import numpy as np


##################
# compute_result #
##################


def compute_result(result_parameter, inputs, fix_point):
    """
    Funciton to generate the expected result of the testcase.

    Arguments
    ---------
    result_parameter: Either OutputArgument or ReturnValue (see pulp_dsp_test.py)
    inputs: Dict mapping name to the Argument, with arg.value, arg.ctype (and arg.length)
    fix_point: None (if no fixpoint is used) or decimal point
    """
    if result_parameter.ctype == 'int32_t':
        a = inputs['srcA'].value.astype(np.int32)
        b = inputs['srcB'].value.astype(np.int32)
        result = np.zeros(1, dtype=np.int32)
        if fix_point is None or fix_point == 0:
            result[0] = np.dot(a, b)
        else:
            # group values and only regularize after grouping
            ctype = inputs['srcA'].ctype
            groups = 2 if ctype == 'int32_t' else 4 if ctype == 'int16_t' else 8
            for g in range(len(a) // groups):
                tmp_val = 0
                for i in range(groups):
                    j = g * groups + i
                    tmp_val = q_add(tmp_val, a[j] * b[j], fix_point)
                result[0] = q_add(result[0], q_roundnorm(tmp_val, fix_point), fix_point)
            # do the remaining elements one by one
            for i in range((len(a) // groups) * groups, len(a)):
                result[0] = q_add(result[0], q_roundnorm(a[i] * b[i], fix_point), fix_point)
    elif result_parameter.ctype == 'float':
        raise RuntimeError("Float not implemented")
    else:
        raise RuntimeError("Unrecognized result type: %s" % result_parameter.ctype)

    return result


######################
# Fixpoint Functions #
######################


def q_sat(x):
    if x > 2**31 - 1:
        return x - 2**32
    elif x < -2**31:
        return x + 2**32
    else:
        return x


def q_add(a, b, p):
    return q_sat(a + b)


def q_sub(a, b, p):
    return q_sat(a - b)


def q_mul(a, b, p):
    return q_sat(q_roundnorm(a * b, p) >> p)


def q_roundnorm(a, p):
    rounding = 1 << (p - 1)
    return q_sat((a + rounding) >> p)


###########################
# generate_stimuli_header #
###########################


if __name__ == "__main__":
    import sys, os
    sys.path.append(os.path.abspath(os.path.join(os.path.realpath(__file__), "../../../..")))
    from pulp_dsp_test import generate_stimuli_header
    generate_stimuli_header(compute_result)