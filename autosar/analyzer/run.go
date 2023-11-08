/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2023  Naive Systems Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

package analyzer

import (
	pb "naive.systems/analyzer/analyzer/proto"
	"naive.systems/analyzer/autosar/rule_A0_1_1"
	"naive.systems/analyzer/autosar/rule_A0_1_2"
	"naive.systems/analyzer/autosar/rule_A0_1_3"
	"naive.systems/analyzer/autosar/rule_A0_1_4"
	"naive.systems/analyzer/autosar/rule_A0_1_5"
	"naive.systems/analyzer/autosar/rule_A0_1_6"
	"naive.systems/analyzer/autosar/rule_A0_4_1"
	"naive.systems/analyzer/autosar/rule_A0_4_2"
	"naive.systems/analyzer/autosar/rule_A0_4_3"
	"naive.systems/analyzer/autosar/rule_A0_4_4"
	"naive.systems/analyzer/autosar/rule_A10_0_1"
	"naive.systems/analyzer/autosar/rule_A10_0_2"
	"naive.systems/analyzer/autosar/rule_A10_1_1"
	"naive.systems/analyzer/autosar/rule_A10_2_1"
	"naive.systems/analyzer/autosar/rule_A10_3_1"
	"naive.systems/analyzer/autosar/rule_A10_3_2"
	"naive.systems/analyzer/autosar/rule_A10_3_3"
	"naive.systems/analyzer/autosar/rule_A10_3_5"
	"naive.systems/analyzer/autosar/rule_A10_4_1"
	"naive.systems/analyzer/autosar/rule_A11_0_1"
	"naive.systems/analyzer/autosar/rule_A11_0_2"
	"naive.systems/analyzer/autosar/rule_A11_3_1"
	"naive.systems/analyzer/autosar/rule_A12_0_1"
	"naive.systems/analyzer/autosar/rule_A12_0_2"
	"naive.systems/analyzer/autosar/rule_A12_1_1"
	"naive.systems/analyzer/autosar/rule_A12_1_2"
	"naive.systems/analyzer/autosar/rule_A12_1_3"
	"naive.systems/analyzer/autosar/rule_A12_1_4"
	"naive.systems/analyzer/autosar/rule_A12_1_5"
	"naive.systems/analyzer/autosar/rule_A12_1_6"
	"naive.systems/analyzer/autosar/rule_A12_4_1"
	"naive.systems/analyzer/autosar/rule_A12_4_2"
	"naive.systems/analyzer/autosar/rule_A12_6_1"
	"naive.systems/analyzer/autosar/rule_A12_7_1"
	"naive.systems/analyzer/autosar/rule_A12_8_1"
	"naive.systems/analyzer/autosar/rule_A12_8_2"
	"naive.systems/analyzer/autosar/rule_A12_8_3"
	"naive.systems/analyzer/autosar/rule_A12_8_4"
	"naive.systems/analyzer/autosar/rule_A12_8_5"
	"naive.systems/analyzer/autosar/rule_A12_8_6"
	"naive.systems/analyzer/autosar/rule_A12_8_7"
	"naive.systems/analyzer/autosar/rule_A13_1_2"
	"naive.systems/analyzer/autosar/rule_A13_1_3"
	"naive.systems/analyzer/autosar/rule_A13_2_1"
	"naive.systems/analyzer/autosar/rule_A13_2_2"
	"naive.systems/analyzer/autosar/rule_A13_2_3"
	"naive.systems/analyzer/autosar/rule_A13_3_1"
	"naive.systems/analyzer/autosar/rule_A13_5_1"
	"naive.systems/analyzer/autosar/rule_A13_5_2"
	"naive.systems/analyzer/autosar/rule_A13_5_3"
	"naive.systems/analyzer/autosar/rule_A13_5_4"
	"naive.systems/analyzer/autosar/rule_A13_5_5"
	"naive.systems/analyzer/autosar/rule_A13_6_1"
	"naive.systems/analyzer/autosar/rule_A14_1_1"
	"naive.systems/analyzer/autosar/rule_A14_5_1"
	"naive.systems/analyzer/autosar/rule_A14_5_2"
	"naive.systems/analyzer/autosar/rule_A14_5_3"
	"naive.systems/analyzer/autosar/rule_A14_7_1"
	"naive.systems/analyzer/autosar/rule_A14_7_2"
	"naive.systems/analyzer/autosar/rule_A14_8_2"
	"naive.systems/analyzer/autosar/rule_A15_0_1"
	"naive.systems/analyzer/autosar/rule_A15_0_2"
	"naive.systems/analyzer/autosar/rule_A15_0_3"
	"naive.systems/analyzer/autosar/rule_A15_0_4"
	"naive.systems/analyzer/autosar/rule_A15_0_5"
	"naive.systems/analyzer/autosar/rule_A15_0_6"
	"naive.systems/analyzer/autosar/rule_A15_0_7"
	"naive.systems/analyzer/autosar/rule_A15_0_8"
	"naive.systems/analyzer/autosar/rule_A15_1_1"
	"naive.systems/analyzer/autosar/rule_A15_1_2"
	"naive.systems/analyzer/autosar/rule_A15_1_3"
	"naive.systems/analyzer/autosar/rule_A15_1_4"
	"naive.systems/analyzer/autosar/rule_A15_1_5"
	"naive.systems/analyzer/autosar/rule_A15_2_1"
	"naive.systems/analyzer/autosar/rule_A15_2_2"
	"naive.systems/analyzer/autosar/rule_A15_3_2"
	"naive.systems/analyzer/autosar/rule_A15_3_3"
	"naive.systems/analyzer/autosar/rule_A15_3_4"
	"naive.systems/analyzer/autosar/rule_A15_3_5"
	"naive.systems/analyzer/autosar/rule_A15_4_1"
	"naive.systems/analyzer/autosar/rule_A15_4_2"
	"naive.systems/analyzer/autosar/rule_A15_4_3"
	"naive.systems/analyzer/autosar/rule_A15_4_4"
	"naive.systems/analyzer/autosar/rule_A15_4_5"
	"naive.systems/analyzer/autosar/rule_A15_5_1"
	"naive.systems/analyzer/autosar/rule_A15_5_2"
	"naive.systems/analyzer/autosar/rule_A15_5_3"
	"naive.systems/analyzer/autosar/rule_A16_0_1"
	"naive.systems/analyzer/autosar/rule_A16_2_1"
	"naive.systems/analyzer/autosar/rule_A16_2_2"
	"naive.systems/analyzer/autosar/rule_A16_2_3"
	"naive.systems/analyzer/autosar/rule_A16_6_1"
	"naive.systems/analyzer/autosar/rule_A16_7_1"
	"naive.systems/analyzer/autosar/rule_A17_0_1"
	"naive.systems/analyzer/autosar/rule_A17_0_2"
	"naive.systems/analyzer/autosar/rule_A17_1_1"
	"naive.systems/analyzer/autosar/rule_A17_6_1"
	"naive.systems/analyzer/autosar/rule_A18_0_1"
	"naive.systems/analyzer/autosar/rule_A18_0_2"
	"naive.systems/analyzer/autosar/rule_A18_0_3"
	"naive.systems/analyzer/autosar/rule_A18_1_1"
	"naive.systems/analyzer/autosar/rule_A18_1_2"
	"naive.systems/analyzer/autosar/rule_A18_1_3"
	"naive.systems/analyzer/autosar/rule_A18_1_4"
	"naive.systems/analyzer/autosar/rule_A18_1_6"
	"naive.systems/analyzer/autosar/rule_A18_5_1"
	"naive.systems/analyzer/autosar/rule_A18_5_10"
	"naive.systems/analyzer/autosar/rule_A18_5_11"
	"naive.systems/analyzer/autosar/rule_A18_5_2"
	"naive.systems/analyzer/autosar/rule_A18_5_3"
	"naive.systems/analyzer/autosar/rule_A18_5_4"
	"naive.systems/analyzer/autosar/rule_A18_5_5"
	"naive.systems/analyzer/autosar/rule_A18_5_6"
	"naive.systems/analyzer/autosar/rule_A18_5_7"
	"naive.systems/analyzer/autosar/rule_A18_5_8"
	"naive.systems/analyzer/autosar/rule_A18_5_9"
	"naive.systems/analyzer/autosar/rule_A18_9_1"
	"naive.systems/analyzer/autosar/rule_A18_9_2"
	"naive.systems/analyzer/autosar/rule_A18_9_3"
	"naive.systems/analyzer/autosar/rule_A18_9_4"
	"naive.systems/analyzer/autosar/rule_A1_1_1"
	"naive.systems/analyzer/autosar/rule_A1_1_2"
	"naive.systems/analyzer/autosar/rule_A1_1_3"
	"naive.systems/analyzer/autosar/rule_A1_2_1"
	"naive.systems/analyzer/autosar/rule_A1_4_1"
	"naive.systems/analyzer/autosar/rule_A1_4_3"
	"naive.systems/analyzer/autosar/rule_A20_8_1"
	"naive.systems/analyzer/autosar/rule_A20_8_2"
	"naive.systems/analyzer/autosar/rule_A20_8_3"
	"naive.systems/analyzer/autosar/rule_A20_8_4"
	"naive.systems/analyzer/autosar/rule_A20_8_5"
	"naive.systems/analyzer/autosar/rule_A20_8_6"
	"naive.systems/analyzer/autosar/rule_A20_8_7"
	"naive.systems/analyzer/autosar/rule_A21_8_1"
	"naive.systems/analyzer/autosar/rule_A23_0_1"
	"naive.systems/analyzer/autosar/rule_A23_0_2"
	"naive.systems/analyzer/autosar/rule_A25_1_1"
	"naive.systems/analyzer/autosar/rule_A25_4_1"
	"naive.systems/analyzer/autosar/rule_A26_5_1"
	"naive.systems/analyzer/autosar/rule_A26_5_2"
	"naive.systems/analyzer/autosar/rule_A27_0_1"
	"naive.systems/analyzer/autosar/rule_A27_0_2"
	"naive.systems/analyzer/autosar/rule_A27_0_3"
	"naive.systems/analyzer/autosar/rule_A27_0_4"
	"naive.systems/analyzer/autosar/rule_A2_10_1"
	"naive.systems/analyzer/autosar/rule_A2_10_4"
	"naive.systems/analyzer/autosar/rule_A2_10_5"
	"naive.systems/analyzer/autosar/rule_A2_10_6"
	"naive.systems/analyzer/autosar/rule_A2_11_1"
	"naive.systems/analyzer/autosar/rule_A2_13_1"
	"naive.systems/analyzer/autosar/rule_A2_13_2"
	"naive.systems/analyzer/autosar/rule_A2_13_3"
	"naive.systems/analyzer/autosar/rule_A2_13_4"
	"naive.systems/analyzer/autosar/rule_A2_13_5"
	"naive.systems/analyzer/autosar/rule_A2_13_6"
	"naive.systems/analyzer/autosar/rule_A2_3_1"
	"naive.systems/analyzer/autosar/rule_A2_5_1"
	"naive.systems/analyzer/autosar/rule_A2_5_2"
	"naive.systems/analyzer/autosar/rule_A2_7_1"
	"naive.systems/analyzer/autosar/rule_A2_7_2"
	"naive.systems/analyzer/autosar/rule_A2_7_3"
	"naive.systems/analyzer/autosar/rule_A2_7_5"
	"naive.systems/analyzer/autosar/rule_A2_8_1"
	"naive.systems/analyzer/autosar/rule_A2_8_2"
	"naive.systems/analyzer/autosar/rule_A3_1_1"
	"naive.systems/analyzer/autosar/rule_A3_1_2"
	"naive.systems/analyzer/autosar/rule_A3_1_3"
	"naive.systems/analyzer/autosar/rule_A3_1_4"
	"naive.systems/analyzer/autosar/rule_A3_1_5"
	"naive.systems/analyzer/autosar/rule_A3_1_6"
	"naive.systems/analyzer/autosar/rule_A3_3_1"
	"naive.systems/analyzer/autosar/rule_A3_3_2"
	"naive.systems/analyzer/autosar/rule_A3_8_1"
	"naive.systems/analyzer/autosar/rule_A3_9_1"
	"naive.systems/analyzer/autosar/rule_A4_10_1"
	"naive.systems/analyzer/autosar/rule_A4_5_1"
	"naive.systems/analyzer/autosar/rule_A4_7_1"
	"naive.systems/analyzer/autosar/rule_A5_0_1"
	"naive.systems/analyzer/autosar/rule_A5_0_2"
	"naive.systems/analyzer/autosar/rule_A5_0_3"
	"naive.systems/analyzer/autosar/rule_A5_0_4"
	"naive.systems/analyzer/autosar/rule_A5_10_1"
	"naive.systems/analyzer/autosar/rule_A5_16_1"
	"naive.systems/analyzer/autosar/rule_A5_1_1"
	"naive.systems/analyzer/autosar/rule_A5_1_2"
	"naive.systems/analyzer/autosar/rule_A5_1_3"
	"naive.systems/analyzer/autosar/rule_A5_1_4"
	"naive.systems/analyzer/autosar/rule_A5_1_6"
	"naive.systems/analyzer/autosar/rule_A5_1_7"
	"naive.systems/analyzer/autosar/rule_A5_1_8"
	"naive.systems/analyzer/autosar/rule_A5_1_9"
	"naive.systems/analyzer/autosar/rule_A5_2_1"
	"naive.systems/analyzer/autosar/rule_A5_2_2"
	"naive.systems/analyzer/autosar/rule_A5_2_3"
	"naive.systems/analyzer/autosar/rule_A5_2_4"
	"naive.systems/analyzer/autosar/rule_A5_2_5"
	"naive.systems/analyzer/autosar/rule_A5_2_6"
	"naive.systems/analyzer/autosar/rule_A5_3_1"
	"naive.systems/analyzer/autosar/rule_A5_3_2"
	"naive.systems/analyzer/autosar/rule_A5_3_3"
	"naive.systems/analyzer/autosar/rule_A5_5_1"
	"naive.systems/analyzer/autosar/rule_A5_6_1"
	"naive.systems/analyzer/autosar/rule_A6_2_1"
	"naive.systems/analyzer/autosar/rule_A6_2_2"
	"naive.systems/analyzer/autosar/rule_A6_4_1"
	"naive.systems/analyzer/autosar/rule_A6_5_1"
	"naive.systems/analyzer/autosar/rule_A6_5_2"
	"naive.systems/analyzer/autosar/rule_A6_5_3"
	"naive.systems/analyzer/autosar/rule_A6_5_4"
	"naive.systems/analyzer/autosar/rule_A6_6_1"
	"naive.systems/analyzer/autosar/rule_A7_1_1"
	"naive.systems/analyzer/autosar/rule_A7_1_2"
	"naive.systems/analyzer/autosar/rule_A7_1_3"
	"naive.systems/analyzer/autosar/rule_A7_1_4"
	"naive.systems/analyzer/autosar/rule_A7_1_5"
	"naive.systems/analyzer/autosar/rule_A7_1_6"
	"naive.systems/analyzer/autosar/rule_A7_1_7"
	"naive.systems/analyzer/autosar/rule_A7_1_8"
	"naive.systems/analyzer/autosar/rule_A7_1_9"
	"naive.systems/analyzer/autosar/rule_A7_2_1"
	"naive.systems/analyzer/autosar/rule_A7_2_2"
	"naive.systems/analyzer/autosar/rule_A7_2_3"
	"naive.systems/analyzer/autosar/rule_A7_2_4"
	"naive.systems/analyzer/autosar/rule_A7_2_5"
	"naive.systems/analyzer/autosar/rule_A7_3_1"
	"naive.systems/analyzer/autosar/rule_A7_4_1"
	"naive.systems/analyzer/autosar/rule_A7_5_1"
	"naive.systems/analyzer/autosar/rule_A7_5_2"
	"naive.systems/analyzer/autosar/rule_A7_6_1"
	"naive.systems/analyzer/autosar/rule_A8_2_1"
	"naive.systems/analyzer/autosar/rule_A8_4_1"
	"naive.systems/analyzer/autosar/rule_A8_4_10"
	"naive.systems/analyzer/autosar/rule_A8_4_11"
	"naive.systems/analyzer/autosar/rule_A8_4_12"
	"naive.systems/analyzer/autosar/rule_A8_4_13"
	"naive.systems/analyzer/autosar/rule_A8_4_14"
	"naive.systems/analyzer/autosar/rule_A8_4_2"
	"naive.systems/analyzer/autosar/rule_A8_4_3"
	"naive.systems/analyzer/autosar/rule_A8_4_4"
	"naive.systems/analyzer/autosar/rule_A8_4_5"
	"naive.systems/analyzer/autosar/rule_A8_4_6"
	"naive.systems/analyzer/autosar/rule_A8_4_7"
	"naive.systems/analyzer/autosar/rule_A8_4_8"
	"naive.systems/analyzer/autosar/rule_A8_4_9"
	"naive.systems/analyzer/autosar/rule_A8_5_0"
	"naive.systems/analyzer/autosar/rule_A8_5_1"
	"naive.systems/analyzer/autosar/rule_A8_5_2"
	"naive.systems/analyzer/autosar/rule_A8_5_3"
	"naive.systems/analyzer/autosar/rule_A8_5_4"
	"naive.systems/analyzer/autosar/rule_A9_3_1"
	"naive.systems/analyzer/autosar/rule_A9_5_1"
	"naive.systems/analyzer/autosar/rule_A9_6_1"
	"naive.systems/analyzer/autosar/rule_A9_6_2"
	"naive.systems/analyzer/autosar/rule_M0_1_1"
	"naive.systems/analyzer/autosar/rule_M0_1_10"
	"naive.systems/analyzer/autosar/rule_M0_1_2"
	"naive.systems/analyzer/autosar/rule_M0_1_3"
	"naive.systems/analyzer/autosar/rule_M0_1_4"
	"naive.systems/analyzer/autosar/rule_M0_1_8"
	"naive.systems/analyzer/autosar/rule_M0_1_9"
	"naive.systems/analyzer/autosar/rule_M0_2_1"
	"naive.systems/analyzer/autosar/rule_M0_3_1"
	"naive.systems/analyzer/autosar/rule_M0_3_2"
	"naive.systems/analyzer/autosar/rule_M0_4_1"
	"naive.systems/analyzer/autosar/rule_M0_4_2"
	"naive.systems/analyzer/autosar/rule_M10_1_1"
	"naive.systems/analyzer/autosar/rule_M10_1_2"
	"naive.systems/analyzer/autosar/rule_M10_1_3"
	"naive.systems/analyzer/autosar/rule_M10_2_1"
	"naive.systems/analyzer/autosar/rule_M10_3_3"
	"naive.systems/analyzer/autosar/rule_M11_0_1"
	"naive.systems/analyzer/autosar/rule_M12_1_1"
	"naive.systems/analyzer/autosar/rule_M14_5_3"
	"naive.systems/analyzer/autosar/rule_M14_6_1"
	"naive.systems/analyzer/autosar/rule_M15_0_3"
	"naive.systems/analyzer/autosar/rule_M15_1_1"
	"naive.systems/analyzer/autosar/rule_M15_1_2"
	"naive.systems/analyzer/autosar/rule_M15_1_3"
	"naive.systems/analyzer/autosar/rule_M15_3_1"
	"naive.systems/analyzer/autosar/rule_M15_3_3"
	"naive.systems/analyzer/autosar/rule_M15_3_4"
	"naive.systems/analyzer/autosar/rule_M15_3_6"
	"naive.systems/analyzer/autosar/rule_M15_3_7"
	"naive.systems/analyzer/autosar/rule_M16_0_1"
	"naive.systems/analyzer/autosar/rule_M16_0_2"
	"naive.systems/analyzer/autosar/rule_M16_0_5"
	"naive.systems/analyzer/autosar/rule_M16_0_6"
	"naive.systems/analyzer/autosar/rule_M16_0_7"
	"naive.systems/analyzer/autosar/rule_M16_0_8"
	"naive.systems/analyzer/autosar/rule_M16_1_1"
	"naive.systems/analyzer/autosar/rule_M16_1_2"
	"naive.systems/analyzer/autosar/rule_M16_2_3"
	"naive.systems/analyzer/autosar/rule_M16_3_1"
	"naive.systems/analyzer/autosar/rule_M16_3_2"
	"naive.systems/analyzer/autosar/rule_M17_0_2"
	"naive.systems/analyzer/autosar/rule_M17_0_3"
	"naive.systems/analyzer/autosar/rule_M17_0_5"
	"naive.systems/analyzer/autosar/rule_M18_0_3"
	"naive.systems/analyzer/autosar/rule_M18_0_4"
	"naive.systems/analyzer/autosar/rule_M18_0_5"
	"naive.systems/analyzer/autosar/rule_M18_2_1"
	"naive.systems/analyzer/autosar/rule_M18_7_1"
	"naive.systems/analyzer/autosar/rule_M19_3_1"
	"naive.systems/analyzer/autosar/rule_M1_0_2"
	"naive.systems/analyzer/autosar/rule_M27_0_1"
	"naive.systems/analyzer/autosar/rule_M2_10_1"
	"naive.systems/analyzer/autosar/rule_M2_13_2"
	"naive.systems/analyzer/autosar/rule_M2_13_3"
	"naive.systems/analyzer/autosar/rule_M2_13_4"
	"naive.systems/analyzer/autosar/rule_M2_7_1"
	"naive.systems/analyzer/autosar/rule_M3_1_2"
	"naive.systems/analyzer/autosar/rule_M3_2_1"
	"naive.systems/analyzer/autosar/rule_M3_2_2"
	"naive.systems/analyzer/autosar/rule_M3_2_3"
	"naive.systems/analyzer/autosar/rule_M3_2_4"
	"naive.systems/analyzer/autosar/rule_M3_3_2"
	"naive.systems/analyzer/autosar/rule_M3_4_1"
	"naive.systems/analyzer/autosar/rule_M3_9_1"
	"naive.systems/analyzer/autosar/rule_M3_9_3"
	"naive.systems/analyzer/autosar/rule_M4_10_1"
	"naive.systems/analyzer/autosar/rule_M4_10_2"
	"naive.systems/analyzer/autosar/rule_M4_5_1"
	"naive.systems/analyzer/autosar/rule_M4_5_3"
	"naive.systems/analyzer/autosar/rule_M5_0_10"
	"naive.systems/analyzer/autosar/rule_M5_0_11"
	"naive.systems/analyzer/autosar/rule_M5_0_12"
	"naive.systems/analyzer/autosar/rule_M5_0_14"
	"naive.systems/analyzer/autosar/rule_M5_0_15"
	"naive.systems/analyzer/autosar/rule_M5_0_16"
	"naive.systems/analyzer/autosar/rule_M5_0_17"
	"naive.systems/analyzer/autosar/rule_M5_0_18"
	"naive.systems/analyzer/autosar/rule_M5_0_2"
	"naive.systems/analyzer/autosar/rule_M5_0_20"
	"naive.systems/analyzer/autosar/rule_M5_0_21"
	"naive.systems/analyzer/autosar/rule_M5_0_3"
	"naive.systems/analyzer/autosar/rule_M5_0_4"
	"naive.systems/analyzer/autosar/rule_M5_0_5"
	"naive.systems/analyzer/autosar/rule_M5_0_6"
	"naive.systems/analyzer/autosar/rule_M5_0_7"
	"naive.systems/analyzer/autosar/rule_M5_0_8"
	"naive.systems/analyzer/autosar/rule_M5_0_9"
	"naive.systems/analyzer/autosar/rule_M5_14_1"
	"naive.systems/analyzer/autosar/rule_M5_17_1"
	"naive.systems/analyzer/autosar/rule_M5_18_1"
	"naive.systems/analyzer/autosar/rule_M5_19_1"
	"naive.systems/analyzer/autosar/rule_M5_2_10"
	"naive.systems/analyzer/autosar/rule_M5_2_11"
	"naive.systems/analyzer/autosar/rule_M5_2_12"
	"naive.systems/analyzer/autosar/rule_M5_2_2"
	"naive.systems/analyzer/autosar/rule_M5_2_3"
	"naive.systems/analyzer/autosar/rule_M5_2_6"
	"naive.systems/analyzer/autosar/rule_M5_2_8"
	"naive.systems/analyzer/autosar/rule_M5_2_9"
	"naive.systems/analyzer/autosar/rule_M5_3_1"
	"naive.systems/analyzer/autosar/rule_M5_3_2"
	"naive.systems/analyzer/autosar/rule_M5_3_3"
	"naive.systems/analyzer/autosar/rule_M5_3_4"
	"naive.systems/analyzer/autosar/rule_M5_8_1"
	"naive.systems/analyzer/autosar/rule_M6_2_1"
	"naive.systems/analyzer/autosar/rule_M6_2_2"
	"naive.systems/analyzer/autosar/rule_M6_2_3"
	"naive.systems/analyzer/autosar/rule_M6_3_1"
	"naive.systems/analyzer/autosar/rule_M6_4_1"
	"naive.systems/analyzer/autosar/rule_M6_4_2"
	"naive.systems/analyzer/autosar/rule_M6_4_3"
	"naive.systems/analyzer/autosar/rule_M6_4_4"
	"naive.systems/analyzer/autosar/rule_M6_4_5"
	"naive.systems/analyzer/autosar/rule_M6_4_6"
	"naive.systems/analyzer/autosar/rule_M6_4_7"
	"naive.systems/analyzer/autosar/rule_M6_5_2"
	"naive.systems/analyzer/autosar/rule_M6_5_3"
	"naive.systems/analyzer/autosar/rule_M6_5_4"
	"naive.systems/analyzer/autosar/rule_M6_5_5"
	"naive.systems/analyzer/autosar/rule_M6_5_6"
	"naive.systems/analyzer/autosar/rule_M6_6_1"
	"naive.systems/analyzer/autosar/rule_M6_6_2"
	"naive.systems/analyzer/autosar/rule_M6_6_3"
	"naive.systems/analyzer/autosar/rule_M7_1_2"
	"naive.systems/analyzer/autosar/rule_M7_3_1"
	"naive.systems/analyzer/autosar/rule_M7_3_2"
	"naive.systems/analyzer/autosar/rule_M7_3_3"
	"naive.systems/analyzer/autosar/rule_M7_3_4"
	"naive.systems/analyzer/autosar/rule_M7_3_6"
	"naive.systems/analyzer/autosar/rule_M7_4_1"
	"naive.systems/analyzer/autosar/rule_M7_4_2"
	"naive.systems/analyzer/autosar/rule_M7_4_3"
	"naive.systems/analyzer/autosar/rule_M7_5_1"
	"naive.systems/analyzer/autosar/rule_M7_5_2"
	"naive.systems/analyzer/autosar/rule_M8_0_1"
	"naive.systems/analyzer/autosar/rule_M8_3_1"
	"naive.systems/analyzer/autosar/rule_M8_4_2"
	"naive.systems/analyzer/autosar/rule_M8_4_4"
	"naive.systems/analyzer/autosar/rule_M8_5_2"
	"naive.systems/analyzer/autosar/rule_M9_3_1"
	"naive.systems/analyzer/autosar/rule_M9_3_3"
	"naive.systems/analyzer/autosar/rule_M9_6_1"
	"naive.systems/analyzer/autosar/rule_M9_6_4"
	"naive.systems/analyzer/cruleslib/options"
	"naive.systems/analyzer/cruleslib/runner"
	"naive.systems/analyzer/misra/checker_integration/checkrule"
	"strings"
)

func Run(rules []checkrule.CheckRule, srcdir string, envOpts *options.EnvOptions) (*pb.ResultsList, []error) {
	taskNums := len(rules)
	numWorkers := envOpts.NumWorkers
	paraTaskRunner := runner.NewParaTaskRunner(numWorkers, taskNums, envOpts.CheckProgress, envOpts.Lang)

	for i, rule := range rules {
		exiting_results, exiting_errors := paraTaskRunner.CheckSignalExiting()
		if exiting_results != nil {
			return exiting_results, exiting_errors
		}

		ruleSpecific := options.NewRuleSpecificOptions(rule.Name, envOpts.ResultsDir)
		ruleOptions := options.MakeCheckOptions(&rule.JSONOptions, envOpts, ruleSpecific)
		x := func(analyze func(srcdir string, opts *options.CheckOptions) (*pb.ResultsList, error)) {
			paraTaskRunner.AddTask(runner.AnalyzerTask{Id: i, Srcdir: srcdir, Opts: &ruleOptions, Rule: rule.Name, Analyze: analyze})
		}
		ruleName := rule.Name
		ruleName = strings.TrimPrefix(ruleName, "autosar/")
		switch ruleName {
		case "rule_M0_1_1":
			x(rule_M0_1_1.Analyze)
		case "rule_M0_1_2":
			x(rule_M0_1_2.Analyze)
		case "rule_M0_1_3":
			x(rule_M0_1_3.Analyze)
		case "rule_M0_1_4":
			x(rule_M0_1_4.Analyze)
		case "rule_A0_1_1":
			x(rule_A0_1_1.Analyze)
		case "rule_A0_1_2":
			x(rule_A0_1_2.Analyze)
		case "rule_M0_1_8":
			x(rule_M0_1_8.Analyze)
		case "rule_M0_1_9":
			x(rule_M0_1_9.Analyze)
		case "rule_M0_1_10":
			x(rule_M0_1_10.Analyze)
		case "rule_A0_1_3":
			x(rule_A0_1_3.Analyze)
		case "rule_A0_1_4":
			x(rule_A0_1_4.Analyze)
		case "rule_A0_1_5":
			x(rule_A0_1_5.Analyze)
		case "rule_A0_1_6":
			x(rule_A0_1_6.Analyze)
		case "rule_M0_2_1":
			x(rule_M0_2_1.Analyze)
		case "rule_M0_3_1":
			x(rule_M0_3_1.Analyze)
		case "rule_M0_3_2":
			x(rule_M0_3_2.Analyze)
		case "rule_M0_4_1":
			x(rule_M0_4_1.Analyze)
		case "rule_M0_4_2":
			x(rule_M0_4_2.Analyze)
		case "rule_A0_4_1":
			x(rule_A0_4_1.Analyze)
		case "rule_A0_4_2":
			x(rule_A0_4_2.Analyze)
		case "rule_A0_4_3":
			x(rule_A0_4_3.Analyze)
		case "rule_A0_4_4":
			x(rule_A0_4_4.Analyze)
		case "rule_A1_1_1":
			x(rule_A1_1_1.Analyze)
		case "rule_M1_0_2":
			x(rule_M1_0_2.Analyze)
		case "rule_A1_1_2":
			x(rule_A1_1_2.Analyze)
		case "rule_A1_1_3":
			x(rule_A1_1_3.Analyze)
		case "rule_A1_2_1":
			x(rule_A1_2_1.Analyze)
		case "rule_A1_4_1":
			x(rule_A1_4_1.Analyze)
		case "rule_A1_4_3":
			x(rule_A1_4_3.Analyze)
		case "rule_A2_3_1":
			x(rule_A2_3_1.Analyze)
		case "rule_A2_5_1":
			x(rule_A2_5_1.Analyze)
		case "rule_A2_5_2":
			x(rule_A2_5_2.Analyze)
		case "rule_M2_7_1":
			x(rule_M2_7_1.Analyze)
		case "rule_A2_7_1":
			x(rule_A2_7_1.Analyze)
		case "rule_A2_7_2":
			x(rule_A2_7_2.Analyze)
		case "rule_A2_7_3":
			x(rule_A2_7_3.Analyze)
		case "rule_A2_7_5":
			x(rule_A2_7_5.Analyze)
		case "rule_A2_8_1":
			x(rule_A2_8_1.Analyze)
		case "rule_A2_8_2":
			x(rule_A2_8_2.Analyze)
		case "rule_M2_10_1":
			x(rule_M2_10_1.Analyze)
		case "rule_A2_10_1":
			x(rule_A2_10_1.Analyze)
		case "rule_A2_10_6":
			x(rule_A2_10_6.Analyze)
		case "rule_A2_10_4":
			x(rule_A2_10_4.Analyze)
		case "rule_A2_10_5":
			x(rule_A2_10_5.Analyze)
		case "rule_A2_11_1":
			x(rule_A2_11_1.Analyze)
		case "rule_A2_13_1":
			x(rule_A2_13_1.Analyze)
		case "rule_A2_13_6":
			x(rule_A2_13_6.Analyze)
		case "rule_A2_13_5":
			x(rule_A2_13_5.Analyze)
		case "rule_M2_13_2":
			x(rule_M2_13_2.Analyze)
		case "rule_M2_13_3":
			x(rule_M2_13_3.Analyze)
		case "rule_M2_13_4":
			x(rule_M2_13_4.Analyze)
		case "rule_A2_13_2":
			x(rule_A2_13_2.Analyze)
		case "rule_A2_13_3":
			x(rule_A2_13_3.Analyze)
		case "rule_A2_13_4":
			x(rule_A2_13_4.Analyze)
		case "rule_A3_1_1":
			x(rule_A3_1_1.Analyze)
		case "rule_A3_1_2":
			x(rule_A3_1_2.Analyze)
		case "rule_A3_1_3":
			x(rule_A3_1_3.Analyze)
		case "rule_M3_1_2":
			x(rule_M3_1_2.Analyze)
		case "rule_A3_1_4":
			x(rule_A3_1_4.Analyze)
		case "rule_A3_1_5":
			x(rule_A3_1_5.Analyze)
		case "rule_A3_1_6":
			x(rule_A3_1_6.Analyze)
		case "rule_M3_2_1":
			x(rule_M3_2_1.Analyze)
		case "rule_M3_2_2":
			x(rule_M3_2_2.Analyze)
		case "rule_M3_2_3":
			x(rule_M3_2_3.Analyze)
		case "rule_M3_2_4":
			x(rule_M3_2_4.Analyze)
		case "rule_A3_3_1":
			x(rule_A3_3_1.Analyze)
		case "rule_A3_3_2":
			x(rule_A3_3_2.Analyze)
		case "rule_M3_3_2":
			x(rule_M3_3_2.Analyze)
		case "rule_M3_4_1":
			x(rule_M3_4_1.Analyze)
		case "rule_A3_8_1":
			x(rule_A3_8_1.Analyze)
		case "rule_M3_9_1":
			x(rule_M3_9_1.Analyze)
		case "rule_A3_9_1":
			x(rule_A3_9_1.Analyze)
		case "rule_M3_9_3":
			x(rule_M3_9_3.Analyze)
		case "rule_M4_5_1":
			x(rule_M4_5_1.Analyze)
		case "rule_A4_5_1":
			x(rule_A4_5_1.Analyze)
		case "rule_M4_5_3":
			x(rule_M4_5_3.Analyze)
		case "rule_A4_7_1":
			x(rule_A4_7_1.Analyze)
		case "rule_M4_10_1":
			x(rule_M4_10_1.Analyze)
		case "rule_A4_10_1":
			x(rule_A4_10_1.Analyze)
		case "rule_M4_10_2":
			x(rule_M4_10_2.Analyze)
		case "rule_A5_0_1":
			x(rule_A5_0_1.Analyze)
		case "rule_M5_0_2":
			x(rule_M5_0_2.Analyze)
		case "rule_M5_0_3":
			x(rule_M5_0_3.Analyze)
		case "rule_M5_0_4":
			x(rule_M5_0_4.Analyze)
		case "rule_M5_0_5":
			x(rule_M5_0_5.Analyze)
		case "rule_M5_0_6":
			x(rule_M5_0_6.Analyze)
		case "rule_M5_0_7":
			x(rule_M5_0_7.Analyze)
		case "rule_M5_0_8":
			x(rule_M5_0_8.Analyze)
		case "rule_M5_0_9":
			x(rule_M5_0_9.Analyze)
		case "rule_M5_0_10":
			x(rule_M5_0_10.Analyze)
		case "rule_M5_0_11":
			x(rule_M5_0_11.Analyze)
		case "rule_M5_0_12":
			x(rule_M5_0_12.Analyze)
		case "rule_A5_0_2":
			x(rule_A5_0_2.Analyze)
		case "rule_M5_0_14":
			x(rule_M5_0_14.Analyze)
		case "rule_M5_0_15":
			x(rule_M5_0_15.Analyze)
		case "rule_M5_0_16":
			x(rule_M5_0_16.Analyze)
		case "rule_M5_0_17":
			x(rule_M5_0_17.Analyze)
		case "rule_A5_0_4":
			x(rule_A5_0_4.Analyze)
		case "rule_M5_0_18":
			x(rule_M5_0_18.Analyze)
		case "rule_A5_0_3":
			x(rule_A5_0_3.Analyze)
		case "rule_M5_0_20":
			x(rule_M5_0_20.Analyze)
		case "rule_M5_0_21":
			x(rule_M5_0_21.Analyze)
		case "rule_A5_1_1":
			x(rule_A5_1_1.Analyze)
		case "rule_A5_1_2":
			x(rule_A5_1_2.Analyze)
		case "rule_A5_1_3":
			x(rule_A5_1_3.Analyze)
		case "rule_A5_1_4":
			x(rule_A5_1_4.Analyze)
		case "rule_A5_1_6":
			x(rule_A5_1_6.Analyze)
		case "rule_A5_1_7":
			x(rule_A5_1_7.Analyze)
		case "rule_A5_1_8":
			x(rule_A5_1_8.Analyze)
		case "rule_A5_1_9":
			x(rule_A5_1_9.Analyze)
		case "rule_M5_2_2":
			x(rule_M5_2_2.Analyze)
		case "rule_M5_2_3":
			x(rule_M5_2_3.Analyze)
		case "rule_A5_2_1":
			x(rule_A5_2_1.Analyze)
		case "rule_A5_2_2":
			x(rule_A5_2_2.Analyze)
		case "rule_A5_2_3":
			x(rule_A5_2_3.Analyze)
		case "rule_M5_2_6":
			x(rule_M5_2_6.Analyze)
		case "rule_A5_2_4":
			x(rule_A5_2_4.Analyze)
		case "rule_A5_2_6":
			x(rule_A5_2_6.Analyze)
		case "rule_M5_2_8":
			x(rule_M5_2_8.Analyze)
		case "rule_M5_2_9":
			x(rule_M5_2_9.Analyze)
		case "rule_M5_2_10":
			x(rule_M5_2_10.Analyze)
		case "rule_M5_2_11":
			x(rule_M5_2_11.Analyze)
		case "rule_A5_2_5":
			x(rule_A5_2_5.Analyze)
		case "rule_M5_2_12":
			x(rule_M5_2_12.Analyze)
		case "rule_M5_3_1":
			x(rule_M5_3_1.Analyze)
		case "rule_M5_3_2":
			x(rule_M5_3_2.Analyze)
		case "rule_M5_3_3":
			x(rule_M5_3_3.Analyze)
		case "rule_M5_3_4":
			x(rule_M5_3_4.Analyze)
		case "rule_A5_3_1":
			x(rule_A5_3_1.Analyze)
		case "rule_A5_3_2":
			x(rule_A5_3_2.Analyze)
		case "rule_A5_3_3":
			x(rule_A5_3_3.Analyze)
		case "rule_A5_5_1":
			x(rule_A5_5_1.Analyze)
		case "rule_A5_6_1":
			x(rule_A5_6_1.Analyze)
		case "rule_M5_8_1":
			x(rule_M5_8_1.Analyze)
		case "rule_A5_10_1":
			x(rule_A5_10_1.Analyze)
		case "rule_M5_14_1":
			x(rule_M5_14_1.Analyze)
		case "rule_A5_16_1":
			x(rule_A5_16_1.Analyze)
		case "rule_M5_17_1":
			x(rule_M5_17_1.Analyze)
		case "rule_M5_18_1":
			x(rule_M5_18_1.Analyze)
		case "rule_M5_19_1":
			x(rule_M5_19_1.Analyze)
		case "rule_M6_2_1":
			x(rule_M6_2_1.Analyze)
		case "rule_A6_2_1":
			x(rule_A6_2_1.Analyze)
		case "rule_A6_2_2":
			x(rule_A6_2_2.Analyze)
		case "rule_M6_2_2":
			x(rule_M6_2_2.Analyze)
		case "rule_M6_2_3":
			x(rule_M6_2_3.Analyze)
		case "rule_M6_3_1":
			x(rule_M6_3_1.Analyze)
		case "rule_M6_4_1":
			x(rule_M6_4_1.Analyze)
		case "rule_M6_4_2":
			x(rule_M6_4_2.Analyze)
		case "rule_M6_4_3":
			x(rule_M6_4_3.Analyze)
		case "rule_M6_4_4":
			x(rule_M6_4_4.Analyze)
		case "rule_M6_4_5":
			x(rule_M6_4_5.Analyze)
		case "rule_M6_4_6":
			x(rule_M6_4_6.Analyze)
		case "rule_M6_4_7":
			x(rule_M6_4_7.Analyze)
		case "rule_A6_4_1":
			x(rule_A6_4_1.Analyze)
		case "rule_A6_5_1":
			x(rule_A6_5_1.Analyze)
		case "rule_A6_5_2":
			x(rule_A6_5_2.Analyze)
		case "rule_M6_5_2":
			x(rule_M6_5_2.Analyze)
		case "rule_M6_5_3":
			x(rule_M6_5_3.Analyze)
		case "rule_M6_5_4":
			x(rule_M6_5_4.Analyze)
		case "rule_M6_5_5":
			x(rule_M6_5_5.Analyze)
		case "rule_M6_5_6":
			x(rule_M6_5_6.Analyze)
		case "rule_A6_5_3":
			x(rule_A6_5_3.Analyze)
		case "rule_A6_5_4":
			x(rule_A6_5_4.Analyze)
		case "rule_A6_6_1":
			x(rule_A6_6_1.Analyze)
		case "rule_M6_6_1":
			x(rule_M6_6_1.Analyze)
		case "rule_M6_6_2":
			x(rule_M6_6_2.Analyze)
		case "rule_M6_6_3":
			x(rule_M6_6_3.Analyze)
		case "rule_A7_1_1":
			x(rule_A7_1_1.Analyze)
		case "rule_A7_1_2":
			x(rule_A7_1_2.Analyze)
		case "rule_M7_1_2":
			x(rule_M7_1_2.Analyze)
		case "rule_A7_1_3":
			x(rule_A7_1_3.Analyze)
		case "rule_A7_1_4":
			x(rule_A7_1_4.Analyze)
		case "rule_A7_1_5":
			x(rule_A7_1_5.Analyze)
		case "rule_A7_1_6":
			x(rule_A7_1_6.Analyze)
		case "rule_A7_1_7":
			x(rule_A7_1_7.Analyze)
		case "rule_A7_1_8":
			x(rule_A7_1_8.Analyze)
		case "rule_A7_1_9":
			x(rule_A7_1_9.Analyze)
		case "rule_A7_2_1":
			x(rule_A7_2_1.Analyze)
		case "rule_A7_2_2":
			x(rule_A7_2_2.Analyze)
		case "rule_A7_2_3":
			x(rule_A7_2_3.Analyze)
		case "rule_A7_2_4":
			x(rule_A7_2_4.Analyze)
		case "rule_A7_2_5":
			x(rule_A7_2_5.Analyze)
		case "rule_M7_3_1":
			x(rule_M7_3_1.Analyze)
		case "rule_M7_3_2":
			x(rule_M7_3_2.Analyze)
		case "rule_M7_3_3":
			x(rule_M7_3_3.Analyze)
		case "rule_M7_3_4":
			x(rule_M7_3_4.Analyze)
		case "rule_A7_3_1":
			x(rule_A7_3_1.Analyze)
		case "rule_M7_3_6":
			x(rule_M7_3_6.Analyze)
		case "rule_A7_4_1":
			x(rule_A7_4_1.Analyze)
		case "rule_M7_4_1":
			x(rule_M7_4_1.Analyze)
		case "rule_M7_4_2":
			x(rule_M7_4_2.Analyze)
		case "rule_M7_4_3":
			x(rule_M7_4_3.Analyze)
		case "rule_M7_5_1":
			x(rule_M7_5_1.Analyze)
		case "rule_M7_5_2":
			x(rule_M7_5_2.Analyze)
		case "rule_A7_5_1":
			x(rule_A7_5_1.Analyze)
		case "rule_A7_5_2":
			x(rule_A7_5_2.Analyze)
		case "rule_A7_6_1":
			x(rule_A7_6_1.Analyze)
		case "rule_M8_0_1":
			x(rule_M8_0_1.Analyze)
		case "rule_A8_2_1":
			x(rule_A8_2_1.Analyze)
		case "rule_M8_3_1":
			x(rule_M8_3_1.Analyze)
		case "rule_A8_4_1":
			x(rule_A8_4_1.Analyze)
		case "rule_M8_4_2":
			x(rule_M8_4_2.Analyze)
		case "rule_A8_4_2":
			x(rule_A8_4_2.Analyze)
		case "rule_M8_4_4":
			x(rule_M8_4_4.Analyze)
		case "rule_A8_4_3":
			x(rule_A8_4_3.Analyze)
		case "rule_A8_4_4":
			x(rule_A8_4_4.Analyze)
		case "rule_A8_4_5":
			x(rule_A8_4_5.Analyze)
		case "rule_A8_4_6":
			x(rule_A8_4_6.Analyze)
		case "rule_A8_4_7":
			x(rule_A8_4_7.Analyze)
		case "rule_A8_4_8":
			x(rule_A8_4_8.Analyze)
		case "rule_A8_4_9":
			x(rule_A8_4_9.Analyze)
		case "rule_A8_4_10":
			x(rule_A8_4_10.Analyze)
		case "rule_A8_4_11":
			x(rule_A8_4_11.Analyze)
		case "rule_A8_4_12":
			x(rule_A8_4_12.Analyze)
		case "rule_A8_4_13":
			x(rule_A8_4_13.Analyze)
		case "rule_A8_4_14":
			x(rule_A8_4_14.Analyze)
		case "rule_A8_5_0":
			x(rule_A8_5_0.Analyze)
		case "rule_A8_5_1":
			x(rule_A8_5_1.Analyze)
		case "rule_M8_5_2":
			x(rule_M8_5_2.Analyze)
		case "rule_A8_5_2":
			x(rule_A8_5_2.Analyze)
		case "rule_A8_5_3":
			x(rule_A8_5_3.Analyze)
		case "rule_A8_5_4":
			x(rule_A8_5_4.Analyze)
		case "rule_M9_3_1":
			x(rule_M9_3_1.Analyze)
		case "rule_A9_3_1":
			x(rule_A9_3_1.Analyze)
		case "rule_M9_3_3":
			x(rule_M9_3_3.Analyze)
		case "rule_A9_5_1":
			x(rule_A9_5_1.Analyze)
		case "rule_M9_6_1":
			x(rule_M9_6_1.Analyze)
		case "rule_A9_6_1":
			x(rule_A9_6_1.Analyze)
		case "rule_A9_6_2":
			x(rule_A9_6_2.Analyze)
		case "rule_M9_6_4":
			x(rule_M9_6_4.Analyze)
		case "rule_A10_0_1":
			x(rule_A10_0_1.Analyze)
		case "rule_A10_0_2":
			x(rule_A10_0_2.Analyze)
		case "rule_A10_1_1":
			x(rule_A10_1_1.Analyze)
		case "rule_M10_1_1":
			x(rule_M10_1_1.Analyze)
		case "rule_M10_1_2":
			x(rule_M10_1_2.Analyze)
		case "rule_M10_1_3":
			x(rule_M10_1_3.Analyze)
		case "rule_M10_2_1":
			x(rule_M10_2_1.Analyze)
		case "rule_A10_2_1":
			x(rule_A10_2_1.Analyze)
		case "rule_A10_3_1":
			x(rule_A10_3_1.Analyze)
		case "rule_A10_3_2":
			x(rule_A10_3_2.Analyze)
		case "rule_A10_3_3":
			x(rule_A10_3_3.Analyze)
		case "rule_A10_3_5":
			x(rule_A10_3_5.Analyze)
		case "rule_M10_3_3":
			x(rule_M10_3_3.Analyze)
		case "rule_A10_4_1":
			x(rule_A10_4_1.Analyze)
		case "rule_M11_0_1":
			x(rule_M11_0_1.Analyze)
		case "rule_A11_0_1":
			x(rule_A11_0_1.Analyze)
		case "rule_A11_0_2":
			x(rule_A11_0_2.Analyze)
		case "rule_A11_3_1":
			x(rule_A11_3_1.Analyze)
		case "rule_A12_0_1":
			x(rule_A12_0_1.Analyze)
		case "rule_A12_0_2":
			x(rule_A12_0_2.Analyze)
		case "rule_A12_1_1":
			x(rule_A12_1_1.Analyze)
		case "rule_M12_1_1":
			x(rule_M12_1_1.Analyze)
		case "rule_A12_1_2":
			x(rule_A12_1_2.Analyze)
		case "rule_A12_1_3":
			x(rule_A12_1_3.Analyze)
		case "rule_A12_1_4":
			x(rule_A12_1_4.Analyze)
		case "rule_A12_1_5":
			x(rule_A12_1_5.Analyze)
		case "rule_A12_1_6":
			x(rule_A12_1_6.Analyze)
		case "rule_A12_4_1":
			x(rule_A12_4_1.Analyze)
		case "rule_A12_4_2":
			x(rule_A12_4_2.Analyze)
		case "rule_A12_6_1":
			x(rule_A12_6_1.Analyze)
		case "rule_A12_7_1":
			x(rule_A12_7_1.Analyze)
		case "rule_A12_8_1":
			x(rule_A12_8_1.Analyze)
		case "rule_A12_8_2":
			x(rule_A12_8_2.Analyze)
		case "rule_A12_8_3":
			x(rule_A12_8_3.Analyze)
		case "rule_A12_8_4":
			x(rule_A12_8_4.Analyze)
		case "rule_A12_8_5":
			x(rule_A12_8_5.Analyze)
		case "rule_A12_8_6":
			x(rule_A12_8_6.Analyze)
		case "rule_A12_8_7":
			x(rule_A12_8_7.Analyze)
		case "rule_A13_1_2":
			x(rule_A13_1_2.Analyze)
		case "rule_A13_1_3":
			x(rule_A13_1_3.Analyze)
		case "rule_A13_2_1":
			x(rule_A13_2_1.Analyze)
		case "rule_A13_2_2":
			x(rule_A13_2_2.Analyze)
		case "rule_A13_2_3":
			x(rule_A13_2_3.Analyze)
		case "rule_A13_3_1":
			x(rule_A13_3_1.Analyze)
		case "rule_A13_5_1":
			x(rule_A13_5_1.Analyze)
		case "rule_A13_5_2":
			x(rule_A13_5_2.Analyze)
		case "rule_A13_5_3":
			x(rule_A13_5_3.Analyze)
		case "rule_A13_5_4":
			x(rule_A13_5_4.Analyze)
		case "rule_A13_5_5":
			x(rule_A13_5_5.Analyze)
		case "rule_A13_6_1":
			x(rule_A13_6_1.Analyze)
		case "rule_A14_1_1":
			x(rule_A14_1_1.Analyze)
		case "rule_A14_5_1":
			x(rule_A14_5_1.Analyze)
		case "rule_A14_5_2":
			x(rule_A14_5_2.Analyze)
		case "rule_A14_5_3":
			x(rule_A14_5_3.Analyze)
		case "rule_M14_5_3":
			x(rule_M14_5_3.Analyze)
		case "rule_M14_6_1":
			x(rule_M14_6_1.Analyze)
		case "rule_A14_7_1":
			x(rule_A14_7_1.Analyze)
		case "rule_A14_7_2":
			x(rule_A14_7_2.Analyze)
		case "rule_A14_8_2":
			x(rule_A14_8_2.Analyze)
		case "rule_A15_0_1":
			x(rule_A15_0_1.Analyze)
		case "rule_A15_0_2":
			x(rule_A15_0_2.Analyze)
		case "rule_A15_0_3":
			x(rule_A15_0_3.Analyze)
		case "rule_A15_0_4":
			x(rule_A15_0_4.Analyze)
		case "rule_A15_0_5":
			x(rule_A15_0_5.Analyze)
		case "rule_A15_0_6":
			x(rule_A15_0_6.Analyze)
		case "rule_A15_0_7":
			x(rule_A15_0_7.Analyze)
		case "rule_A15_0_8":
			x(rule_A15_0_8.Analyze)
		case "rule_A15_1_1":
			x(rule_A15_1_1.Analyze)
		case "rule_A15_1_2":
			x(rule_A15_1_2.Analyze)
		case "rule_M15_0_3":
			x(rule_M15_0_3.Analyze)
		case "rule_M15_1_1":
			x(rule_M15_1_1.Analyze)
		case "rule_M15_1_2":
			x(rule_M15_1_2.Analyze)
		case "rule_M15_1_3":
			x(rule_M15_1_3.Analyze)
		case "rule_A15_1_3":
			x(rule_A15_1_3.Analyze)
		case "rule_A15_1_4":
			x(rule_A15_1_4.Analyze)
		case "rule_A15_1_5":
			x(rule_A15_1_5.Analyze)
		case "rule_A15_2_1":
			x(rule_A15_2_1.Analyze)
		case "rule_A15_2_2":
			x(rule_A15_2_2.Analyze)
		case "rule_M15_3_1":
			x(rule_M15_3_1.Analyze)
		case "rule_A15_3_2":
			x(rule_A15_3_2.Analyze)
		case "rule_A15_3_3":
			x(rule_A15_3_3.Analyze)
		case "rule_A15_3_4":
			x(rule_A15_3_4.Analyze)
		case "rule_M15_3_3":
			x(rule_M15_3_3.Analyze)
		case "rule_M15_3_4":
			x(rule_M15_3_4.Analyze)
		case "rule_A15_3_5":
			x(rule_A15_3_5.Analyze)
		case "rule_M15_3_6":
			x(rule_M15_3_6.Analyze)
		case "rule_M15_3_7":
			x(rule_M15_3_7.Analyze)
		case "rule_A15_4_1":
			x(rule_A15_4_1.Analyze)
		case "rule_A15_4_2":
			x(rule_A15_4_2.Analyze)
		case "rule_A15_4_3":
			x(rule_A15_4_3.Analyze)
		case "rule_A15_4_4":
			x(rule_A15_4_4.Analyze)
		case "rule_A15_4_5":
			x(rule_A15_4_5.Analyze)
		case "rule_A15_5_1":
			x(rule_A15_5_1.Analyze)
		case "rule_A15_5_2":
			x(rule_A15_5_2.Analyze)
		case "rule_A15_5_3":
			x(rule_A15_5_3.Analyze)
		case "rule_A16_0_1":
			x(rule_A16_0_1.Analyze)
		case "rule_M16_0_1":
			x(rule_M16_0_1.Analyze)
		case "rule_M16_0_2":
			x(rule_M16_0_2.Analyze)
		case "rule_M16_0_5":
			x(rule_M16_0_5.Analyze)
		case "rule_M16_0_6":
			x(rule_M16_0_6.Analyze)
		case "rule_M16_0_7":
			x(rule_M16_0_7.Analyze)
		case "rule_M16_0_8":
			x(rule_M16_0_8.Analyze)
		case "rule_M16_1_1":
			x(rule_M16_1_1.Analyze)
		case "rule_M16_1_2":
			x(rule_M16_1_2.Analyze)
		case "rule_M16_2_3":
			x(rule_M16_2_3.Analyze)
		case "rule_A16_2_1":
			x(rule_A16_2_1.Analyze)
		case "rule_A16_2_2":
			x(rule_A16_2_2.Analyze)
		case "rule_A16_2_3":
			x(rule_A16_2_3.Analyze)
		case "rule_M16_3_1":
			x(rule_M16_3_1.Analyze)
		case "rule_M16_3_2":
			x(rule_M16_3_2.Analyze)
		case "rule_A16_6_1":
			x(rule_A16_6_1.Analyze)
		case "rule_A16_7_1":
			x(rule_A16_7_1.Analyze)
		case "rule_A17_0_1":
			x(rule_A17_0_1.Analyze)
		case "rule_M17_0_2":
			x(rule_M17_0_2.Analyze)
		case "rule_M17_0_3":
			x(rule_M17_0_3.Analyze)
		case "rule_A17_0_2":
			x(rule_A17_0_2.Analyze)
		case "rule_M17_0_5":
			x(rule_M17_0_5.Analyze)
		case "rule_A17_1_1":
			x(rule_A17_1_1.Analyze)
		case "rule_A17_6_1":
			x(rule_A17_6_1.Analyze)
		case "rule_A18_0_1":
			x(rule_A18_0_1.Analyze)
		case "rule_A18_0_2":
			x(rule_A18_0_2.Analyze)
		case "rule_M18_0_3":
			x(rule_M18_0_3.Analyze)
		case "rule_M18_0_4":
			x(rule_M18_0_4.Analyze)
		case "rule_M18_0_5":
			x(rule_M18_0_5.Analyze)
		case "rule_A18_0_3":
			x(rule_A18_0_3.Analyze)
		case "rule_A18_1_1":
			x(rule_A18_1_1.Analyze)
		case "rule_A18_1_2":
			x(rule_A18_1_2.Analyze)
		case "rule_A18_1_3":
			x(rule_A18_1_3.Analyze)
		case "rule_A18_1_4":
			x(rule_A18_1_4.Analyze)
		case "rule_A18_1_6":
			x(rule_A18_1_6.Analyze)
		case "rule_M18_2_1":
			x(rule_M18_2_1.Analyze)
		case "rule_A18_5_1":
			x(rule_A18_5_1.Analyze)
		case "rule_A18_5_2":
			x(rule_A18_5_2.Analyze)
		case "rule_A18_5_3":
			x(rule_A18_5_3.Analyze)
		case "rule_A18_5_4":
			x(rule_A18_5_4.Analyze)
		case "rule_A18_5_5":
			x(rule_A18_5_5.Analyze)
		case "rule_A18_5_6":
			x(rule_A18_5_6.Analyze)
		case "rule_A18_5_7":
			x(rule_A18_5_7.Analyze)
		case "rule_A18_5_8":
			x(rule_A18_5_8.Analyze)
		case "rule_A18_5_9":
			x(rule_A18_5_9.Analyze)
		case "rule_A18_5_10":
			x(rule_A18_5_10.Analyze)
		case "rule_A18_5_11":
			x(rule_A18_5_11.Analyze)
		case "rule_M18_7_1":
			x(rule_M18_7_1.Analyze)
		case "rule_A18_9_1":
			x(rule_A18_9_1.Analyze)
		case "rule_A18_9_2":
			x(rule_A18_9_2.Analyze)
		case "rule_A18_9_3":
			x(rule_A18_9_3.Analyze)
		case "rule_A18_9_4":
			x(rule_A18_9_4.Analyze)
		case "rule_M19_3_1":
			x(rule_M19_3_1.Analyze)
		case "rule_A20_8_1":
			x(rule_A20_8_1.Analyze)
		case "rule_A20_8_2":
			x(rule_A20_8_2.Analyze)
		case "rule_A20_8_3":
			x(rule_A20_8_3.Analyze)
		case "rule_A20_8_4":
			x(rule_A20_8_4.Analyze)
		case "rule_A20_8_5":
			x(rule_A20_8_5.Analyze)
		case "rule_A20_8_6":
			x(rule_A20_8_6.Analyze)
		case "rule_A20_8_7":
			x(rule_A20_8_7.Analyze)
		case "rule_A21_8_1":
			x(rule_A21_8_1.Analyze)
		case "rule_A23_0_1":
			x(rule_A23_0_1.Analyze)
		case "rule_A23_0_2":
			x(rule_A23_0_2.Analyze)
		case "rule_A25_1_1":
			x(rule_A25_1_1.Analyze)
		case "rule_A25_4_1":
			x(rule_A25_4_1.Analyze)
		case "rule_A26_5_1":
			x(rule_A26_5_1.Analyze)
		case "rule_A26_5_2":
			x(rule_A26_5_2.Analyze)
		case "rule_M27_0_1":
			x(rule_M27_0_1.Analyze)
		case "rule_A27_0_1":
			x(rule_A27_0_1.Analyze)
		case "rule_A27_0_4":
			x(rule_A27_0_4.Analyze)
		case "rule_A27_0_2":
			x(rule_A27_0_2.Analyze)
		case "rule_A27_0_3":
			x(rule_A27_0_3.Analyze)
		}
	}
	return paraTaskRunner.CollectResultsAndErrors()
}
