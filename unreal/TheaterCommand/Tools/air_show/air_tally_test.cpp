// The tallies' test (Battle/AirTally): which mark goes in which slot of an aircraft's record, against the modeller's
// reference (tools/battle-models/tallies.py on claude/project-thread-3vdnfu, battle-models/air/tally/README.md). The
// README's two worked examples, then fifty records laid out by the reference itself (ten chosen, forty drawn at random:
// own nations that paint their own symbol, kills past thirty, kills with no nation, more nations than rows, missions
// past the slots), each mark's slot, symbol or tin, and fade to a thousandth. Then a fighter's own marks to a row (the
// Spitfire's nine), and an ace's name under his cockpit (battle-models/air-next/ACES.md, acepanel.py): his title as the
// rules give it in the forms that may be painted, its ink for the paint under it, the letters' sheet
// (Tools/name_letters.py) holding every capital the rules' names use, and every name fitting every fighter's line.
// Usage: air_tally_test [name-letters.json]
#include "Battle/AirTally.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	struct Case
	{
		const char* Nation;
		int Missions, Kills;
		const char* By;
		int MissionSlots, KillSlots, Seat;
		const char* Marks;
	};

	// (made by tallies.fill, 22 November 2026: "B<slot>:<tin>:<fade>" a bomb, "K<slot>:<symbol>:<fade>" a kill)
	const Case Cases[] = {
	{ "USA", 0, 12, "GER=8,ITA=4", 100, 30, 0, "K0:GER:0.850 K1:GER:0.871 K2:GER:0.893 K3:GER:0.914 K4:GER:0.936 K5:GER:0.957 K6:GER:0.979 K7:GER:1.000 K10:ITA:0.850 K11:ITA:0.900 K12:ITA:0.950 K13:ITA:1.000" },
	{ "USA", 0, 11, "GER=5,ITA=3,SOV=1,UK=2", 100, 30, 0, "K0:GER:0.850 K1:GER:0.887 K2:GER:0.925 K3:GER:0.963 K4:GER:1.000 K10:ITA:0.850 K11:ITA:0.925 K12:ITA:1.000 K20:UK:0.850 K21:UK:0.925 K23:SOV:1.000" },
	{ "SOV", 0, 17, "GER=17", 0, 30, 0, "K0:SOV:0.850 K1:SOV:0.867 K2:SOV:0.883 K3:SOV:0.900 K4:SOV:0.917 K5:SOV:0.933 K6:SOV:0.950 K7:SOV:0.967 K8:SOV:0.983 K9:SOV:1.000 K10:SOV:0.850 K11:SOV:0.875 K12:SOV:0.900 K13:SOV:0.925 K14:SOV:0.950 K15:SOV:0.975 K16:SOV:1.000" },
	{ "JAP", 0, 5, "", 0, 30, 0, "K0:BLOSSOM:0.850 K1:BLOSSOM:0.887 K2:BLOSSOM:0.925 K3:BLOSSOM:0.963 K4:BLOSSOM:1.000" },
	{ "UK", 25, 3, "GER=2,ITA=1", 100, 30, 1, "B0:3:0.670 B1:3:0.684 B2:3:0.698 B3:3:0.711 B4:3:0.725 B5:3:0.739 B6:3:0.753 B7:3:0.766 B8:3:0.780 B9:3:0.794 B10:2:0.807 B11:2:0.821 B12:2:0.835 B13:2:0.849 B14:2:0.863 B15:2:0.876 B16:2:0.890 B17:2:0.904 B18:2:0.917 B19:2:0.931 B20:1:0.945 B21:1:0.959 B22:1:0.972 B23:1:0.986 B24:1:1.000 K0:GER:0.850 K1:GER:1.000 K10:ITA:1.000" },
	{ "GER", 63, 4, "SOV=1,UK=3", 100, 30, 2, "B0:0:0.670 B1:0:0.675 B2:0:0.681 B3:0:0.686 B4:0:0.691 B5:0:0.697 B6:0:0.702 B7:0:0.707 B8:0:0.713 B9:0:0.718 B10:3:0.723 B11:3:0.729 B12:3:0.734 B13:3:0.739 B14:3:0.745 B15:3:0.750 B16:3:0.755 B17:3:0.760 B18:3:0.766 B19:3:0.771 B20:2:0.776 B21:2:0.782 B22:2:0.787 B23:2:0.792 B24:2:0.798 B25:2:0.803 B26:2:0.808 B27:2:0.814 B28:2:0.819 B29:2:0.824 B30:1:0.830 B31:1:0.835 B32:1:0.840 B33:1:0.846 B34:1:0.851 B35:1:0.856 B36:1:0.862 B37:1:0.867 B38:1:0.872 B39:1:0.878 B40:0:0.883 B41:0:0.888 B42:0:0.894 B43:0:0.899 B44:0:0.904 B45:0:0.910 B46:0:0.915 B47:0:0.920 B48:0:0.925 B49:0:0.931 B50:3:0.936 B51:3:0.941 B52:3:0.947 B53:3:0.952 B54:3:0.957 B55:3:0.963 B56:3:0.968 B57:3:0.973 B58:3:0.979 B59:3:0.984 B60:2:0.989 B61:2:0.995 B62:2:1.000 K0:UK:0.850 K1:UK:0.925 K2:UK:1.000 K10:SOV:1.000" },
	{ "USA", 0, 40, "GER=40", 0, 30, 0, "K0:GER:0.850 K1:GER:0.867 K2:GER:0.883 K3:GER:0.900 K4:GER:0.917 K5:GER:0.933 K6:GER:0.950 K7:GER:0.967 K8:GER:0.983 K9:GER:1.000 K10:GER:0.850 K11:GER:0.867 K12:GER:0.883 K13:GER:0.900 K14:GER:0.917 K15:GER:0.933 K16:GER:0.950 K17:GER:0.967 K18:GER:0.983 K19:GER:1.000 K20:GER:0.850 K21:GER:0.867 K22:GER:0.883 K23:GER:0.900 K24:GER:0.917 K25:GER:0.933 K26:GER:0.950 K27:GER:0.967 K28:GER:0.983 K29:GER:1.000" },
	{ "USA", 0, 7, "", 0, 30, 0, "K0:PLANE:0.850 K1:PLANE:0.875 K2:PLANE:0.900 K3:PLANE:0.925 K4:PLANE:0.950 K5:PLANE:0.975 K6:PLANE:1.000" },
	{ "USA", 120, 0, "", 100, 30, 3, "B0:0:0.670 B1:0:0.673 B2:0:0.677 B3:0:0.680 B4:0:0.683 B5:0:0.687 B6:0:0.690 B7:0:0.693 B8:0:0.697 B9:0:0.700 B10:3:0.703 B11:3:0.707 B12:3:0.710 B13:3:0.713 B14:3:0.717 B15:3:0.720 B16:3:0.723 B17:3:0.727 B18:3:0.730 B19:3:0.733 B20:2:0.737 B21:2:0.740 B22:2:0.743 B23:2:0.747 B24:2:0.750 B25:2:0.753 B26:2:0.757 B27:2:0.760 B28:2:0.763 B29:2:0.767 B30:1:0.770 B31:1:0.773 B32:1:0.777 B33:1:0.780 B34:1:0.783 B35:1:0.787 B36:1:0.790 B37:1:0.793 B38:1:0.797 B39:1:0.800 B40:0:0.803 B41:0:0.807 B42:0:0.810 B43:0:0.813 B44:0:0.817 B45:0:0.820 B46:0:0.823 B47:0:0.827 B48:0:0.830 B49:0:0.833 B50:3:0.837 B51:3:0.840 B52:3:0.843 B53:3:0.847 B54:3:0.850 B55:3:0.853 B56:3:0.857 B57:3:0.860 B58:3:0.863 B59:3:0.867 B60:2:0.870 B61:2:0.873 B62:2:0.877 B63:2:0.880 B64:2:0.883 B65:2:0.887 B66:2:0.890 B67:2:0.893 B68:2:0.897 B69:2:0.900 B70:1:0.903 B71:1:0.907 B72:1:0.910 B73:1:0.913 B74:1:0.917 B75:1:0.920 B76:1:0.923 B77:1:0.927 B78:1:0.930 B79:1:0.933 B80:0:0.937 B81:0:0.940 B82:0:0.943 B83:0:0.947 B84:0:0.950 B85:0:0.953 B86:0:0.957 B87:0:0.960 B88:0:0.963 B89:0:0.967 B90:3:0.970 B91:3:0.973 B92:3:0.977 B93:3:0.980 B94:3:0.983 B95:3:0.987 B96:3:0.990 B97:3:0.993 B98:3:0.997 B99:3:1.000" },
	{ "USA", 1, 1, "GER=1", 100, 30, 0, "B0:0:1.000 K0:GER:1.000" },
	{ "UK", 100, 2, "FIN=2,ITA=2,JAP=3", 0, 30, 1, "K0:JAP:0.850 K1:JAP:0.925 K2:JAP:1.000 K10:ITA:0.850 K11:ITA:1.000 K20:PLANE:0.850 K21:PLANE:1.000" },
	{ "ITA", 12, 13, "CAN=6", 100, 30, 6, "B0:3:0.670 B1:3:0.700 B2:3:0.730 B3:3:0.760 B4:3:0.790 B5:3:0.820 B6:3:0.850 B7:3:0.880 B8:3:0.910 B9:3:0.940 B10:2:0.970 B11:2:1.000 K0:UK:0.850 K1:UK:0.867 K2:UK:0.883 K3:UK:0.900 K4:UK:0.917 K5:UK:0.933 K6:UK:0.950 K7:UK:0.967 K8:UK:0.983 K9:UK:1.000 K10:UK:0.850 K11:UK:0.925 K12:UK:1.000" },
	{ "JAP", 0, 10, "GER=3,ROM=11", 100, 20, 7, "K0:BLOSSOM:0.850 K1:BLOSSOM:0.867 K2:BLOSSOM:0.883 K3:BLOSSOM:0.900 K4:BLOSSOM:0.917 K5:BLOSSOM:0.933 K6:BLOSSOM:0.950 K7:BLOSSOM:0.967 K8:BLOSSOM:0.983 K9:BLOSSOM:1.000" },
	{ "HUN", 50, 11, "FRA=5,ROM=6,USA=3", 100, 30, 7, "B0:3:0.670 B1:3:0.677 B2:3:0.683 B3:3:0.690 B4:3:0.697 B5:3:0.704 B6:3:0.710 B7:3:0.717 B8:3:0.724 B9:3:0.731 B10:2:0.737 B11:2:0.744 B12:2:0.751 B13:2:0.758 B14:2:0.764 B15:2:0.771 B16:2:0.778 B17:2:0.784 B18:2:0.791 B19:2:0.798 B20:1:0.805 B21:1:0.811 B22:1:0.818 B23:1:0.825 B24:1:0.832 B25:1:0.838 B26:1:0.845 B27:1:0.852 B28:1:0.859 B29:1:0.865 B30:0:0.872 B31:0:0.879 B32:0:0.886 B33:0:0.892 B34:0:0.899 B35:0:0.906 B36:0:0.912 B37:0:0.919 B38:0:0.926 B39:0:0.933 B40:3:0.939 B41:3:0.946 B42:3:0.953 B43:3:0.960 B44:3:0.966 B45:3:0.973 B46:3:0.980 B47:3:0.987 B48:3:0.993 B49:3:1.000 K0:PLANE:0.850 K1:PLANE:0.880 K2:PLANE:0.910 K3:PLANE:0.940 K4:PLANE:0.970 K5:PLANE:1.000 K10:FRA:0.850 K11:FRA:0.887 K12:FRA:0.925 K13:FRA:0.963 K14:FRA:1.000 K20:USA:0.850 K21:USA:0.925 K22:USA:1.000" },
	{ "FIN", 130, 1, "CAN=5,GER=3,UK=3,USA=2", 0, 20, 9, "K0:UK:0.850 K1:UK:0.871 K2:UK:0.893 K3:UK:0.914 K4:UK:0.936 K5:UK:0.957 K6:UK:0.979 K7:UK:1.000 K10:GER:0.850 K11:GER:0.925 K12:GER:1.000" },
	{ "UK", 37, 23, "HUN=1,USA=4", 50, 30, 8, "B0:3:0.670 B1:3:0.679 B2:3:0.688 B3:3:0.698 B4:3:0.707 B5:3:0.716 B6:3:0.725 B7:3:0.734 B8:3:0.743 B9:3:0.753 B10:2:0.762 B11:2:0.771 B12:2:0.780 B13:2:0.789 B14:2:0.798 B15:2:0.807 B16:2:0.817 B17:2:0.826 B18:2:0.835 B19:2:0.844 B20:1:0.853 B21:1:0.863 B22:1:0.872 B23:1:0.881 B24:1:0.890 B25:1:0.899 B26:1:0.908 B27:1:0.917 B28:1:0.927 B29:1:0.936 B30:0:0.945 B31:0:0.954 B32:0:0.963 B33:0:0.972 B34:0:0.982 B35:0:0.991 B36:0:1.000 K0:USA:0.850 K1:USA:0.867 K2:USA:0.883 K3:USA:0.900 K4:USA:0.917 K5:USA:0.933 K6:USA:0.950 K7:USA:0.967 K8:USA:0.983 K9:USA:1.000 K10:USA:0.850 K11:USA:0.867 K12:USA:0.883 K13:USA:0.900 K14:USA:0.917 K15:USA:0.933 K16:USA:0.950 K17:USA:0.967 K18:USA:0.983 K19:USA:1.000 K20:USA:0.850 K21:USA:0.925 K23:PLANE:1.000" },
	{ "ITA", 100, 11, "GER=4,HUN=1,ITA=11,SOV=7,USA=6", 100, 20, 0, "B0:1:0.670 B1:1:0.673 B2:1:0.677 B3:1:0.680 B4:1:0.683 B5:1:0.687 B6:1:0.690 B7:1:0.693 B8:1:0.697 B9:1:0.700 B10:0:0.703 B11:0:0.707 B12:0:0.710 B13:0:0.713 B14:0:0.717 B15:0:0.720 B16:0:0.723 B17:0:0.727 B18:0:0.730 B19:0:0.733 B20:3:0.737 B21:3:0.740 B22:3:0.743 B23:3:0.747 B24:3:0.750 B25:3:0.753 B26:3:0.757 B27:3:0.760 B28:3:0.763 B29:3:0.767 B30:2:0.770 B31:2:0.773 B32:2:0.777 B33:2:0.780 B34:2:0.783 B35:2:0.787 B36:2:0.790 B37:2:0.793 B38:2:0.797 B39:2:0.800 B40:1:0.803 B41:1:0.807 B42:1:0.810 B43:1:0.813 B44:1:0.817 B45:1:0.820 B46:1:0.823 B47:1:0.827 B48:1:0.830 B49:1:0.833 B50:0:0.837 B51:0:0.840 B52:0:0.843 B53:0:0.847 B54:0:0.850 B55:0:0.853 B56:0:0.857 B57:0:0.860 B58:0:0.863 B59:0:0.867 B60:3:0.870 B61:3:0.873 B62:3:0.877 B63:3:0.880 B64:3:0.883 B65:3:0.887 B66:3:0.890 B67:3:0.893 B68:3:0.897 B69:3:0.900 B70:2:0.903 B71:2:0.907 B72:2:0.910 B73:2:0.913 B74:2:0.917 B75:2:0.920 B76:2:0.923 B77:2:0.927 B78:2:0.930 B79:2:0.933 B80:1:0.937 B81:1:0.940 B82:1:0.943 B83:1:0.947 B84:1:0.950 B85:1:0.953 B86:1:0.957 B87:1:0.960 B88:1:0.963 B89:1:0.967 B90:0:0.970 B91:0:0.973 B92:0:0.977 B93:0:0.980 B94:0:0.983 B95:0:0.987 B96:0:0.990 B97:0:0.993 B98:0:0.997 B99:0:1.000 K0:ITA:0.850 K1:ITA:0.867 K2:ITA:0.883 K3:ITA:0.900 K4:ITA:0.917 K5:ITA:0.933 K6:ITA:0.950 K7:ITA:0.967 K8:ITA:0.983 K9:ITA:1.000 K10:ITA:0.850 K12:SOV:0.871 K13:SOV:0.893 K14:SOV:0.914 K15:SOV:0.936 K16:SOV:0.957 K17:SOV:0.979 K18:SOV:1.000" },
	{ "CAN", 50, 34, "FRA=8,GER=7,HUN=4,ITA=1", 0, 12, 3, "K0:FRA:0.850 K1:FRA:0.867 K2:FRA:0.883 K3:FRA:0.900 K4:FRA:0.917 K5:FRA:0.933 K6:FRA:0.950 K7:FRA:0.967 K8:FRA:0.983 K9:FRA:1.000 K10:FRA:0.850 K11:FRA:0.867" },
	{ "VIC", 130, 13, "ITA=6,SOV=13,UK=2,VIC=3", 100, 12, 1, "B0:2:0.670 B1:2:0.673 B2:2:0.677 B3:2:0.680 B4:2:0.683 B5:2:0.687 B6:2:0.690 B7:2:0.693 B8:2:0.697 B9:2:0.700 B10:1:0.703 B11:1:0.707 B12:1:0.710 B13:1:0.713 B14:1:0.717 B15:1:0.720 B16:1:0.723 B17:1:0.727 B18:1:0.730 B19:1:0.733 B20:0:0.737 B21:0:0.740 B22:0:0.743 B23:0:0.747 B24:0:0.750 B25:0:0.753 B26:0:0.757 B27:0:0.760 B28:0:0.763 B29:0:0.767 B30:3:0.770 B31:3:0.773 B32:3:0.777 B33:3:0.780 B34:3:0.783 B35:3:0.787 B36:3:0.790 B37:3:0.793 B38:3:0.797 B39:3:0.800 B40:2:0.803 B41:2:0.807 B42:2:0.810 B43:2:0.813 B44:2:0.817 B45:2:0.820 B46:2:0.823 B47:2:0.827 B48:2:0.830 B49:2:0.833 B50:1:0.837 B51:1:0.840 B52:1:0.843 B53:1:0.847 B54:1:0.850 B55:1:0.853 B56:1:0.857 B57:1:0.860 B58:1:0.863 B59:1:0.867 B60:0:0.870 B61:0:0.873 B62:0:0.877 B63:0:0.880 B64:0:0.883 B65:0:0.887 B66:0:0.890 B67:0:0.893 B68:0:0.897 B69:0:0.900 B70:3:0.903 B71:3:0.907 B72:3:0.910 B73:3:0.913 B74:3:0.917 B75:3:0.920 B76:3:0.923 B77:3:0.927 B78:3:0.930 B79:3:0.933 B80:2:0.937 B81:2:0.940 B82:2:0.943 B83:2:0.947 B84:2:0.950 B85:2:0.953 B86:2:0.957 B87:2:0.960 B88:2:0.963 B89:2:0.967 B90:1:0.970 B91:1:0.973 B92:1:0.977 B93:1:0.980 B94:1:0.983 B95:1:0.987 B96:1:0.990 B97:1:0.993 B98:1:0.997 B99:1:1.000 K0:SOV:0.850 K1:SOV:0.867 K2:SOV:0.883 K3:SOV:0.900 K4:SOV:0.917 K5:SOV:0.933 K6:SOV:0.950 K7:SOV:0.967 K8:SOV:0.983 K9:SOV:1.000 K10:SOV:0.850 K11:SOV:0.869" },
	{ "JAP", 37, 1, "CAN=1,JAP=1,VIC=2", 0, 30, 6, "K0:BLOSSOM:1.000" },
	{ "GER", 0, 23, "FIN=3,GER=2,ITA=4,UK=14", 50, 30, 5, "K0:UK:0.850 K1:UK:0.867 K2:UK:0.883 K3:UK:0.900 K4:UK:0.917 K5:UK:0.933 K6:UK:0.950 K7:UK:0.967 K8:UK:0.983 K9:UK:1.000 K10:UK:0.850 K11:UK:0.871 K12:UK:0.893 K13:UK:0.914 K15:ITA:0.936 K16:ITA:0.957 K17:ITA:0.979 K18:ITA:1.000 K20:PLANE:0.850 K21:PLANE:0.887 K22:PLANE:0.925 K24:GER:0.963 K25:GER:1.000" },
	{ "VIC", 99, 45, "HUN=9,ITA=2,JAP=12,UK=2", 50, 30, 6, "B0:2:0.670 B1:2:0.677 B2:2:0.683 B3:2:0.690 B4:2:0.697 B5:2:0.704 B6:2:0.710 B7:2:0.717 B8:2:0.724 B9:2:0.731 B10:1:0.737 B11:1:0.744 B12:1:0.751 B13:1:0.758 B14:1:0.764 B15:1:0.771 B16:1:0.778 B17:1:0.784 B18:1:0.791 B19:1:0.798 B20:0:0.805 B21:0:0.811 B22:0:0.818 B23:0:0.825 B24:0:0.832 B25:0:0.838 B26:0:0.845 B27:0:0.852 B28:0:0.859 B29:0:0.865 B30:3:0.872 B31:3:0.879 B32:3:0.886 B33:3:0.892 B34:3:0.899 B35:3:0.906 B36:3:0.912 B37:3:0.919 B38:3:0.926 B39:3:0.933 B40:2:0.939 B41:2:0.946 B42:2:0.953 B43:2:0.960 B44:2:0.966 B45:2:0.973 B46:2:0.980 B47:2:0.987 B48:2:0.993 B49:2:1.000 K0:JAP:0.850 K1:JAP:0.867 K2:JAP:0.883 K3:JAP:0.900 K4:JAP:0.917 K5:JAP:0.933 K6:JAP:0.950 K7:JAP:0.967 K8:JAP:0.983 K9:JAP:1.000 K10:JAP:0.850 K11:JAP:0.867 K12:JAP:0.883 K13:JAP:0.900 K14:JAP:0.917 K15:JAP:0.933 K16:JAP:0.950 K17:JAP:0.967 K18:JAP:0.983 K19:JAP:1.000 K20:JAP:0.850 K21:JAP:0.867 K22:JAP:0.883 K23:JAP:0.900 K24:JAP:0.917 K25:JAP:0.933 K26:JAP:0.950 K27:JAP:0.967 K28:JAP:0.983 K29:JAP:1.000" },
	{ "FIN", 12, 34, "", 50, 20, 8, "B0:1:0.670 B1:1:0.700 B2:1:0.730 B3:1:0.760 B4:1:0.790 B5:1:0.820 B6:1:0.850 B7:1:0.880 B8:1:0.910 B9:1:0.940 B10:0:0.970 B11:0:1.000 K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:PLANE:0.850 K11:PLANE:0.867 K12:PLANE:0.883 K13:PLANE:0.900 K14:PLANE:0.917 K15:PLANE:0.933 K16:PLANE:0.950 K17:PLANE:0.967 K18:PLANE:0.983 K19:PLANE:1.000" },
	{ "VIC", 51, 0, "", 50, 30, 4, "B0:0:0.670 B1:0:0.677 B2:0:0.683 B3:0:0.690 B4:0:0.697 B5:0:0.704 B6:0:0.710 B7:0:0.717 B8:0:0.724 B9:0:0.731 B10:3:0.737 B11:3:0.744 B12:3:0.751 B13:3:0.758 B14:3:0.764 B15:3:0.771 B16:3:0.778 B17:3:0.784 B18:3:0.791 B19:3:0.798 B20:2:0.805 B21:2:0.811 B22:2:0.818 B23:2:0.825 B24:2:0.832 B25:2:0.838 B26:2:0.845 B27:2:0.852 B28:2:0.859 B29:2:0.865 B30:1:0.872 B31:1:0.879 B32:1:0.886 B33:1:0.892 B34:1:0.899 B35:1:0.906 B36:1:0.912 B37:1:0.919 B38:1:0.926 B39:1:0.933 B40:0:0.939 B41:0:0.946 B42:0:0.953 B43:0:0.960 B44:0:0.966 B45:0:0.973 B46:0:0.980 B47:0:0.987 B48:0:0.993 B49:0:1.000" },
	{ "HUN", 0, 11, "", 50, 12, 4, "K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:PLANE:1.000" },
	{ "FRA", 130, 21, "CAN=9,FIN=4,GER=3,HUN=9", 0, 20, 7, "K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:PLANE:0.850 K11:PLANE:0.880 K12:PLANE:0.910 K14:GER:0.940 K15:GER:0.970 K16:GER:1.000" },
	{ "GER", 99, 30, "", 100, 20, 1, "B0:2:0.670 B1:2:0.673 B2:2:0.677 B3:2:0.680 B4:2:0.683 B5:2:0.687 B6:2:0.690 B7:2:0.694 B8:2:0.697 B9:2:0.700 B10:1:0.704 B11:1:0.707 B12:1:0.710 B13:1:0.714 B14:1:0.717 B15:1:0.721 B16:1:0.724 B17:1:0.727 B18:1:0.731 B19:1:0.734 B20:0:0.737 B21:0:0.741 B22:0:0.744 B23:0:0.747 B24:0:0.751 B25:0:0.754 B26:0:0.758 B27:0:0.761 B28:0:0.764 B29:0:0.768 B30:3:0.771 B31:3:0.774 B32:3:0.778 B33:3:0.781 B34:3:0.784 B35:3:0.788 B36:3:0.791 B37:3:0.795 B38:3:0.798 B39:3:0.801 B40:2:0.805 B41:2:0.808 B42:2:0.811 B43:2:0.815 B44:2:0.818 B45:2:0.822 B46:2:0.825 B47:2:0.828 B48:2:0.832 B49:2:0.835 B50:1:0.838 B51:1:0.842 B52:1:0.845 B53:1:0.848 B54:1:0.852 B55:1:0.855 B56:1:0.859 B57:1:0.862 B58:1:0.865 B59:1:0.869 B60:0:0.872 B61:0:0.875 B62:0:0.879 B63:0:0.882 B64:0:0.886 B65:0:0.889 B66:0:0.892 B67:0:0.896 B68:0:0.899 B69:0:0.902 B70:3:0.906 B71:3:0.909 B72:3:0.912 B73:3:0.916 B74:3:0.919 B75:3:0.923 B76:3:0.926 B77:3:0.929 B78:3:0.933 B79:3:0.936 B80:2:0.939 B81:2:0.943 B82:2:0.946 B83:2:0.949 B84:2:0.953 B85:2:0.956 B86:2:0.960 B87:2:0.963 B88:2:0.966 B89:2:0.970 B90:1:0.973 B91:1:0.976 B92:1:0.980 B93:1:0.983 B94:1:0.987 B95:1:0.990 B96:1:0.993 B97:1:0.997 B98:1:1.000 K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:PLANE:0.850 K11:PLANE:0.867 K12:PLANE:0.883 K13:PLANE:0.900 K14:PLANE:0.917 K15:PLANE:0.933 K16:PLANE:0.950 K17:PLANE:0.967 K18:PLANE:0.983 K19:PLANE:1.000" },
	{ "USA", 50, 0, "GER=5,ITA=7,SOV=16", 0, 30, 1, "" },
	{ "UK", 5, 30, "JAP=3,USA=4", 0, 12, 2, "K0:USA:0.850 K1:USA:0.867 K2:USA:0.883 K3:USA:0.900 K4:USA:0.917 K5:USA:0.933 K6:USA:0.950 K7:USA:0.967 K8:USA:0.983 K9:USA:1.000 K10:USA:0.850 K11:USA:0.867" },
	{ "VIC", 51, 17, "ITA=6,VIC=7", 50, 30, 4, "B0:0:0.670 B1:0:0.677 B2:0:0.683 B3:0:0.690 B4:0:0.697 B5:0:0.704 B6:0:0.710 B7:0:0.717 B8:0:0.724 B9:0:0.731 B10:3:0.737 B11:3:0.744 B12:3:0.751 B13:3:0.758 B14:3:0.764 B15:3:0.771 B16:3:0.778 B17:3:0.784 B18:3:0.791 B19:3:0.798 B20:2:0.805 B21:2:0.811 B22:2:0.818 B23:2:0.825 B24:2:0.832 B25:2:0.838 B26:2:0.845 B27:2:0.852 B28:2:0.859 B29:2:0.865 B30:1:0.872 B31:1:0.879 B32:1:0.886 B33:1:0.892 B34:1:0.899 B35:1:0.906 B36:1:0.912 B37:1:0.919 B38:1:0.926 B39:1:0.933 B40:0:0.939 B41:0:0.946 B42:0:0.953 B43:0:0.960 B44:0:0.966 B45:0:0.973 B46:0:0.980 B47:0:0.987 B48:0:0.993 B49:0:1.000 K0:FRA:0.850 K1:FRA:0.867 K2:FRA:0.883 K3:FRA:0.900 K4:FRA:0.917 K5:FRA:0.933 K6:FRA:0.950 K7:FRA:0.967 K8:FRA:0.983 K9:FRA:1.000 K10:FRA:1.000 K20:ITA:0.850 K21:ITA:0.880 K22:ITA:0.910 K23:ITA:0.940 K24:ITA:0.970 K25:ITA:1.000" },
	{ "CAN", 1, 45, "CAN=8,FRA=9,HUN=1,SOV=6", 50, 30, 8, "B0:0:1.000 K0:FRA:0.850 K1:FRA:0.867 K2:FRA:0.883 K3:FRA:0.900 K4:FRA:0.917 K5:FRA:0.933 K6:FRA:0.950 K7:FRA:0.967 K8:FRA:0.983 K9:FRA:1.000 K10:FRA:0.850 K11:FRA:0.867 K12:FRA:0.883 K13:FRA:0.900 K14:FRA:0.917 K15:FRA:0.933 K16:FRA:0.950 K17:FRA:0.967 K18:FRA:0.983 K19:FRA:1.000 K20:FRA:0.850 K21:FRA:0.867 K22:FRA:0.883 K23:FRA:0.900 K24:FRA:0.917 K25:FRA:0.933 K26:FRA:0.950 K27:FRA:0.967 K28:FRA:0.983 K29:FRA:1.000" },
	{ "FRA", 12, 13, "", 100, 30, 5, "B0:2:0.670 B1:2:0.700 B2:2:0.730 B3:2:0.760 B4:2:0.790 B5:2:0.820 B6:2:0.850 B7:2:0.880 B8:2:0.910 B9:2:0.940 B10:1:0.970 B11:1:1.000 K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:PLANE:0.850 K11:PLANE:0.925 K12:PLANE:1.000" },
	{ "FIN", 0, 17, "CAN=1,UK=9", 50, 30, 7, "K0:UK:0.850 K1:UK:0.867 K2:UK:0.883 K3:UK:0.900 K4:UK:0.917 K5:UK:0.933 K6:UK:0.950 K7:UK:0.967 K8:UK:0.983 K9:UK:1.000 K10:UK:0.850 K11:UK:0.875 K12:UK:0.900 K13:UK:0.925 K14:UK:0.950 K15:UK:0.975 K16:UK:1.000" },
	{ "ITA", 12, 45, "FIN=3,GER=3,ROM=1,USA=6", 0, 20, 7, "K0:USA:0.850 K1:USA:0.867 K2:USA:0.883 K3:USA:0.900 K4:USA:0.917 K5:USA:0.933 K6:USA:0.950 K7:USA:0.967 K8:USA:0.983 K9:USA:1.000 K10:USA:0.850 K11:USA:0.867 K12:USA:0.883 K13:USA:0.900 K14:USA:0.917 K15:USA:0.933 K16:USA:0.950 K17:USA:0.967 K18:USA:0.983 K19:USA:1.000" },
	{ "UK", 51, 11, "GER=2", 100, 12, 6, "B0:3:0.670 B1:3:0.677 B2:3:0.683 B3:3:0.690 B4:3:0.696 B5:3:0.703 B6:3:0.710 B7:3:0.716 B8:3:0.723 B9:3:0.729 B10:2:0.736 B11:2:0.743 B12:2:0.749 B13:2:0.756 B14:2:0.762 B15:2:0.769 B16:2:0.776 B17:2:0.782 B18:2:0.789 B19:2:0.795 B20:1:0.802 B21:1:0.809 B22:1:0.815 B23:1:0.822 B24:1:0.828 B25:1:0.835 B26:1:0.842 B27:1:0.848 B28:1:0.855 B29:1:0.861 B30:0:0.868 B31:0:0.875 B32:0:0.881 B33:0:0.888 B34:0:0.894 B35:0:0.901 B36:0:0.908 B37:0:0.914 B38:0:0.921 B39:0:0.927 B40:3:0.934 B41:3:0.941 B42:3:0.947 B43:3:0.954 B44:3:0.960 B45:3:0.967 B46:3:0.974 B47:3:0.980 B48:3:0.987 B49:3:0.993 B50:2:1.000 K0:GER:0.850 K1:GER:0.867 K2:GER:0.883 K3:GER:0.900 K4:GER:0.917 K5:GER:0.933 K6:GER:0.950 K7:GER:0.967 K8:GER:0.983 K9:GER:1.000 K10:GER:1.000" },
	{ "VIC", 0, 8, "FRA=7,JAP=2", 100, 20, 8, "K0:FRA:0.850 K1:FRA:0.875 K2:FRA:0.900 K3:FRA:0.925 K4:FRA:0.950 K5:FRA:0.975 K6:FRA:1.000 K10:JAP:0.850 K11:JAP:1.000" },
	{ "FRA", 100, 11, "FIN=5,ROM=1,UK=8,VIC=3", 100, 30, 2, "B0:3:0.670 B1:3:0.673 B2:3:0.677 B3:3:0.680 B4:3:0.683 B5:3:0.687 B6:3:0.690 B7:3:0.693 B8:3:0.697 B9:3:0.700 B10:2:0.703 B11:2:0.707 B12:2:0.710 B13:2:0.713 B14:2:0.717 B15:2:0.720 B16:2:0.723 B17:2:0.727 B18:2:0.730 B19:2:0.733 B20:1:0.737 B21:1:0.740 B22:1:0.743 B23:1:0.747 B24:1:0.750 B25:1:0.753 B26:1:0.757 B27:1:0.760 B28:1:0.763 B29:1:0.767 B30:0:0.770 B31:0:0.773 B32:0:0.777 B33:0:0.780 B34:0:0.783 B35:0:0.787 B36:0:0.790 B37:0:0.793 B38:0:0.797 B39:0:0.800 B40:3:0.803 B41:3:0.807 B42:3:0.810 B43:3:0.813 B44:3:0.817 B45:3:0.820 B46:3:0.823 B47:3:0.827 B48:3:0.830 B49:3:0.833 B50:2:0.837 B51:2:0.840 B52:2:0.843 B53:2:0.847 B54:2:0.850 B55:2:0.853 B56:2:0.857 B57:2:0.860 B58:2:0.863 B59:2:0.867 B60:1:0.870 B61:1:0.873 B62:1:0.877 B63:1:0.880 B64:1:0.883 B65:1:0.887 B66:1:0.890 B67:1:0.893 B68:1:0.897 B69:1:0.900 B70:0:0.903 B71:0:0.907 B72:0:0.910 B73:0:0.913 B74:0:0.917 B75:0:0.920 B76:0:0.923 B77:0:0.927 B78:0:0.930 B79:0:0.933 B80:3:0.937 B81:3:0.940 B82:3:0.943 B83:3:0.947 B84:3:0.950 B85:3:0.953 B86:3:0.957 B87:3:0.960 B88:3:0.963 B89:3:0.967 B90:2:0.970 B91:2:0.973 B92:2:0.977 B93:2:0.980 B94:2:0.983 B95:2:0.987 B96:2:0.990 B97:2:0.993 B98:2:0.997 B99:2:1.000 K0:UK:0.850 K1:UK:0.871 K2:UK:0.893 K3:UK:0.914 K4:UK:0.936 K5:UK:0.957 K6:UK:0.979 K7:UK:1.000 K10:PLANE:0.850 K11:PLANE:0.880 K12:PLANE:0.910 K13:PLANE:0.940 K14:PLANE:0.970 K15:PLANE:1.000 K20:FRA:0.850 K21:FRA:0.925 K22:FRA:1.000" },
	{ "HUN", 1, 21, "HUN=20,ITA=1", 50, 30, 7, "B0:3:1.000 K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:PLANE:0.850 K11:PLANE:0.867 K12:PLANE:0.883 K13:PLANE:0.900 K14:PLANE:0.917 K15:PLANE:0.933 K16:PLANE:0.950 K17:PLANE:0.967 K18:PLANE:0.983 K19:PLANE:1.000 K20:ITA:1.000" },
	{ "VIC", 100, 11, "CAN=1,FRA=13,HUN=7,ROM=9,UK=7", 50, 20, 2, "B0:2:0.670 B1:2:0.677 B2:2:0.683 B3:2:0.690 B4:2:0.697 B5:2:0.704 B6:2:0.710 B7:2:0.717 B8:2:0.724 B9:2:0.731 B10:1:0.737 B11:1:0.744 B12:1:0.751 B13:1:0.758 B14:1:0.764 B15:1:0.771 B16:1:0.778 B17:1:0.784 B18:1:0.791 B19:1:0.798 B20:0:0.805 B21:0:0.811 B22:0:0.818 B23:0:0.825 B24:0:0.832 B25:0:0.838 B26:0:0.845 B27:0:0.852 B28:0:0.859 B29:0:0.865 B30:3:0.872 B31:3:0.879 B32:3:0.886 B33:3:0.892 B34:3:0.899 B35:3:0.906 B36:3:0.912 B37:3:0.919 B38:3:0.926 B39:3:0.933 B40:2:0.939 B41:2:0.946 B42:2:0.953 B43:2:0.960 B44:2:0.966 B45:2:0.973 B46:2:0.980 B47:2:0.987 B48:2:0.993 B49:2:1.000 K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:PLANE:0.850 K11:PLANE:0.869 K12:PLANE:0.887 K13:PLANE:0.906 K14:PLANE:0.925 K15:PLANE:0.944 K17:FRA:0.963 K18:FRA:0.981 K19:FRA:1.000" },
	{ "VIC", 12, 3, "FRA=6,GER=3,JAP=6,VIC=5", 50, 20, 1, "B0:2:0.670 B1:2:0.700 B2:2:0.730 B3:2:0.760 B4:2:0.790 B5:2:0.820 B6:2:0.850 B7:2:0.880 B8:2:0.910 B9:2:0.940 B10:1:0.970 B11:1:1.000 K0:FRA:0.850 K1:FRA:0.867 K2:FRA:0.883 K3:FRA:0.900 K4:FRA:0.917 K5:FRA:0.933 K6:FRA:0.950 K7:FRA:0.967 K8:FRA:0.983 K9:FRA:1.000 K10:FRA:1.000" },
	{ "FRA", 50, 17, "FIN=5", 50, 12, 2, "B0:2:0.670 B1:2:0.677 B2:2:0.683 B3:2:0.690 B4:2:0.697 B5:2:0.704 B6:2:0.710 B7:2:0.717 B8:2:0.724 B9:2:0.731 B10:1:0.737 B11:1:0.744 B12:1:0.751 B13:1:0.758 B14:1:0.764 B15:1:0.771 B16:1:0.778 B17:1:0.784 B18:1:0.791 B19:1:0.798 B20:0:0.805 B21:0:0.811 B22:0:0.818 B23:0:0.825 B24:0:0.832 B25:0:0.838 B26:0:0.845 B27:0:0.852 B28:0:0.859 B29:0:0.865 B30:3:0.872 B31:3:0.879 B32:3:0.886 B33:3:0.892 B34:3:0.899 B35:3:0.906 B36:3:0.912 B37:3:0.919 B38:3:0.926 B39:3:0.933 B40:2:0.939 B41:2:0.946 B42:2:0.953 B43:2:0.960 B44:2:0.966 B45:2:0.973 B46:2:0.980 B47:2:0.987 B48:2:0.993 B49:2:1.000 K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:PLANE:0.850 K11:PLANE:0.875" },
	{ "CAN", 5, 0, "GER=9,HUN=5,JAP=8,ROM=7,UK=9", 0, 30, 2, "" },
	{ "CAN", 12, 17, "CAN=7,ROM=10,USA=4,VIC=1", 100, 20, 4, "B0:1:0.670 B1:1:0.700 B2:1:0.730 B3:1:0.760 B4:1:0.790 B5:1:0.820 B6:1:0.850 B7:1:0.880 B8:1:0.910 B9:1:0.940 B10:0:0.970 B11:0:1.000 K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:UK:0.850 K11:UK:0.875 K12:UK:0.900 K13:UK:0.925 K14:UK:0.950 K15:UK:0.975 K16:UK:1.000" },
	{ "FIN", 0, 8, "", 100, 12, 5, "K0:PLANE:0.850 K1:PLANE:0.871 K2:PLANE:0.893 K3:PLANE:0.914 K4:PLANE:0.936 K5:PLANE:0.957 K6:PLANE:0.979 K7:PLANE:1.000" },
	{ "ITA", 5, 10, "CAN=4,ROM=2", 0, 30, 3, "K0:UK:0.850 K1:UK:0.871 K2:UK:0.893 K3:UK:0.914 K4:UK:0.936 K5:UK:0.957 K6:UK:0.979 K7:UK:1.000 K10:PLANE:0.850 K11:PLANE:1.000" },
	{ "VIC", 12, 21, "ROM=13,SOV=6", 50, 20, 4, "B0:1:0.670 B1:1:0.700 B2:1:0.730 B3:1:0.760 B4:1:0.790 B5:1:0.820 B6:1:0.850 B7:1:0.880 B8:1:0.910 B9:1:0.940 B10:0:0.970 B11:0:1.000 K0:PLANE:0.850 K1:PLANE:0.867 K2:PLANE:0.883 K3:PLANE:0.900 K4:PLANE:0.917 K5:PLANE:0.933 K6:PLANE:0.950 K7:PLANE:0.967 K8:PLANE:0.983 K9:PLANE:1.000 K10:PLANE:0.850 K11:PLANE:0.887 K12:PLANE:0.925 K13:PLANE:0.963 K14:PLANE:1.000" },
	{ "JAP", 51, 5, "SOV=7,VIC=5", 0, 30, 5, "K0:BLOSSOM:0.850 K1:BLOSSOM:0.887 K2:BLOSSOM:0.925 K3:BLOSSOM:0.963 K4:BLOSSOM:1.000" },
	{ "ROM", 51, 2, "CAN=8,FIN=1,FRA=2,JAP=7,VIC=7", 50, 30, 0, "B0:0:0.670 B1:0:0.677 B2:0:0.683 B3:0:0.690 B4:0:0.697 B5:0:0.704 B6:0:0.710 B7:0:0.717 B8:0:0.724 B9:0:0.731 B10:3:0.737 B11:3:0.744 B12:3:0.751 B13:3:0.758 B14:3:0.764 B15:3:0.771 B16:3:0.778 B17:3:0.784 B18:3:0.791 B19:3:0.798 B20:2:0.805 B21:2:0.811 B22:2:0.818 B23:2:0.825 B24:2:0.832 B25:2:0.838 B26:2:0.845 B27:2:0.852 B28:2:0.859 B29:2:0.865 B30:1:0.872 B31:1:0.879 B32:1:0.886 B33:1:0.892 B34:1:0.899 B35:1:0.906 B36:1:0.912 B37:1:0.919 B38:1:0.926 B39:1:0.933 B40:0:0.939 B41:0:0.946 B42:0:0.953 B43:0:0.960 B44:0:0.966 B45:0:0.973 B46:0:0.980 B47:0:0.987 B48:0:0.993 B49:0:1.000 K0:FRA:0.850 K1:FRA:0.869 K2:FRA:0.887 K3:FRA:0.906 K4:FRA:0.925 K5:FRA:0.944 K6:FRA:0.963 K7:FRA:0.981 K8:FRA:1.000 K10:UK:0.850 K11:UK:0.871 K12:UK:0.893 K13:UK:0.914 K14:UK:0.936 K15:UK:0.957 K16:UK:0.979 K17:UK:1.000 K20:JAP:0.850 K21:JAP:0.871 K22:JAP:0.893 K23:JAP:0.914 K24:JAP:0.936 K25:JAP:0.957 K26:JAP:0.979 K28:PLANE:1.000" },
	{ "ROM", 0, 13, "FRA=7,HUN=9,ITA=3,SOV=8,VIC=9", 100, 12, 0, "K0:FRA:0.850 K1:FRA:0.867 K2:FRA:0.883 K3:FRA:0.900 K4:FRA:0.917 K5:FRA:0.933 K6:FRA:0.950 K7:FRA:0.967 K8:FRA:0.983 K9:FRA:1.000 K10:FRA:0.850 K11:FRA:0.869" },
	{ "HUN", 37, 11, "JAP=16,UK=6", 100, 20, 0, "B0:3:0.670 B1:3:0.679 B2:3:0.688 B3:3:0.698 B4:3:0.707 B5:3:0.716 B6:3:0.725 B7:3:0.734 B8:3:0.743 B9:3:0.753 B10:2:0.762 B11:2:0.771 B12:2:0.780 B13:2:0.789 B14:2:0.798 B15:2:0.807 B16:2:0.817 B17:2:0.826 B18:2:0.835 B19:2:0.844 B20:1:0.853 B21:1:0.863 B22:1:0.872 B23:1:0.881 B24:1:0.890 B25:1:0.899 B26:1:0.908 B27:1:0.917 B28:1:0.927 B29:1:0.936 B30:0:0.945 B31:0:0.954 B32:0:0.963 B33:0:0.972 B34:0:0.982 B35:0:0.991 B36:0:1.000 K0:JAP:0.850 K1:JAP:0.867 K2:JAP:0.883 K3:JAP:0.900 K4:JAP:0.917 K5:JAP:0.933 K6:JAP:0.950 K7:JAP:0.967 K8:JAP:0.983 K9:JAP:1.000 K10:JAP:0.850 K11:JAP:0.880 K12:JAP:0.910 K13:JAP:0.940 K14:JAP:0.970 K15:JAP:1.000" },
	};

	std::map<std::string, int> ByOf(const std::string& Text)
	{
		std::map<std::string, int> Out;
		std::stringstream In(Text);
		std::string One;
		while (std::getline(In, One, ','))
		{
			const size_t Eq = One.find('=');
			if (Eq != std::string::npos) Out[One.substr(0, Eq)] = std::atoi(One.c_str() + Eq + 1);
		}
		return Out;
	}

	std::string Laid(const std::vector<AirTally::Mark>& Marks)
	{
		std::string Out;
		char Buf[64];
		for (const AirTally::Mark& M : Marks)
		{
			if (M.bKill) std::snprintf(Buf, sizeof(Buf), "K%d:%s:%.4f", M.Slot, M.Tag.substr(5).c_str(), M.Fade);
			else std::snprintf(Buf, sizeof(Buf), "B%d:%d:%.4f", M.Slot, M.Tin, M.Fade);
			Out += (Out.empty() ? "" : " ") + std::string(Buf);
		}
		return Out;
	}

	/** The same marks, fades to within half a thousandth (the reference rounds them to three places). */
	bool Same(const std::string& Want, const std::vector<AirTally::Mark>& Got, std::string& Why)
	{
		std::stringstream In(Want);
		std::string One;
		size_t K = 0;
		while (In >> One)
		{
			if (K >= Got.size()) { Why = "too few marks"; return false; }
			const AirTally::Mark& M = Got[K++];
			const size_t A = One.find(':'), B = One.rfind(':');
			const std::string Slot = One.substr(1, A - 1), What = One.substr(A + 1, B - A - 1);
			const double Fade = std::atof(One.c_str() + B + 1);
			const bool bKill = One[0] == 'K';
			const bool bOk = bKill == M.bKill && std::atoi(Slot.c_str()) == M.Slot && std::fabs(Fade - M.Fade) <= 0.0006
				&& (bKill ? ("KILL-" + What) == M.Tag : std::atoi(What.c_str()) == M.Tin);
			if (!bOk) { Why = "mark " + std::to_string(K - 1) + " wants " + One; return false; }
		}
		if (K != Got.size()) { Why = "too many marks"; return false; }
		return true;
	}
}

namespace
{
	int Check(bool bOk, const char* What, const std::string& Got)
	{
		std::printf("  %s  %s%s%s\n", bOk ? "ok  " : "FAIL", What, Got.empty() ? "" : ": ", Got.c_str());
		return bOk ? 0 : 1;
	}

	/** Each fighter's name line and kill rows as the modeller staged them (battle-models/air-next, after the art
	 * director's 19:47Z pass): her name, the capitals' height, its least, the line's length, her kills to a row and her
	 * kill slots. */
	struct Line
	{
		const char* Name;
		double Cap, CapMin, Long;
		int PerRow, Slots;
	};
	const Line Lines[] = {
		{ "Bf109", 0.04, 0.035, 1.18, 10, 30 },
		{ "C202", 0.05, 0.035, 1.18, 10, 30 },
		{ "CarrierFighter", 0.07, 0.035, 1.18, 10, 30 },
		{ "Fighter", 0.07, 0.035, 1.18, 10, 30 },
		{ "Fw190", 0.05, 0.035, 1.18, 10, 30 },
		{ "Hellcat", 0.05, 0.035, 1.18, 10, 30 },
		{ "Hurricane", 0.07, 0.035, 1.18, 10, 30 },
		{ "Me262", 0.04, 0.035, 1.18, 10, 30 },
		{ "Mustang", 0.05, 0.035, 1.18, 10, 30 },
		{ "Spitfire", 0.05, 0.035, 1.06, 9, 27 },
		{ "Warhawk", 0.07, 0.035, 1.18, 10, 30 },
		{ "WhatIf_EscortFighter", 0.05, 0.035, 1.18, 10, 30 },
		{ "WhatIf_NightFighter", 0.05, 0.035, 1.18, 10, 30 },
		{ "Wildcat", 0.05, 0.035, 1.18, 10, 30 },
		{ "Yak", 0.05, 0.035, 1.18, 10, 30 },
		{ "Zero", 0.05, 0.035, 1.18, 10, 30 },
	};

	/** A number in a model file: the first after Key at or after From (no JSON reader here; the files are the modeller's
	 * own, one key order). At is left after it; npos where there is none. */
	double NumberAfter(const std::string& Text, const std::string& Key, size_t From, size_t& At)
	{
		At = From == std::string::npos ? std::string::npos : Text.find("\"" + Key + "\":", From);
		if (At == std::string::npos) return 0.0;
		At += Key.size() + 3;
		return std::strtod(Text.c_str() + At, nullptr);
	}

	/** How many slots a list of them holds, [[x, y, z, nx, ny, nz], ...], from its opening bracket. */
	int SlotsAt(const std::string& Text, size_t Open)
	{
		int Depth = 0, Count = 0;
		for (size_t K = Open; K < Text.size(); ++K)
		{
			if (Text[K] == '[' && ++Depth == 2) ++Count;
			if (Text[K] == ']' && --Depth == 0) break;
		}
		return Count;
	}

	/** Titles as the rules give them (aces.ts aceTitle, aces.json's ranks and names), and what is painted. */
	struct Title
	{
		const char* Nation;
		const char* Given;
		const char* Painted;
	};
	const Title Titles[] = {
		{ "UK", "Flight Lieutenant Tom Harland", "F/LT T. HARLAND" },
		{ "CAN", "Pilot Officer Harry Pryce", "P/O H. PRYCE" },
		{ "AUS", "Wing Commander Arthur Dawlish", "W/CDR A. DAWLISH" },
		{ "USA", "Captain Don Merrick", "CAPT. D. MERRICK" },
		{ "USA", "Lieutenant Colonel Jack Calder", "LT. COL. J. CALDER" },
		{ "USA", "Lieutenant Ray Ashford", "LT. R. ASHFORD" },
		// (a carrier group's ace: the rules' navy lists, 4a9caaee)
		{ "USA", "Ensign Tom Harland", "ENS. T. HARLAND" },
		{ "USA", "Lieutenant (junior grade) Walt Brenner", "LT.(JG) W. BRENNER" },
		{ "USA", "Lieutenant Commander Ed Mallory", "LCDR. E. MALLORY" },
		{ "UK", "Sub-Lieutenant Peter Lisle", "S/LT P. LISLE" },
		{ "UK", "Lieutenant Hugh Carver", "LT H. CARVER" },
		{ "UK", "Lieutenant Commander Robert Allingham", "LT CDR R. ALLINGHAM" },
		{ "UK", "Commander Giles Ashby", "CDR G. ASHBY" },
		{ "GER", "Oberleutnant Kurt Vossler", "OBLT. K. VOSSLER" },
		{ "GER", "Feldwebel Hans Albrecht", "FW. H. ALBRECHT" },
		{ "SOV", "Senior Lieutenant Ivan Veresov", "ST. LT. I. VERESOV" },
		{ "SOV", "Lieutenant Colonel Pyotr Gromov", "P. GROMOV" },
		{ "JAP", "Petty Officer Takeo Kawano", "T. KAWANO" },
		{ "JAP", "Lieutenant Commander Kenji Matsuda", "K. MATSUDA" },
		{ "ITA", "Tenente Giorgio Rossetti", "TEN. G. ROSSETTI" },
		{ "ITA", "Sergente Maggiore Luigi Galli", "SERG. MAGG. L. GALLI" },
		{ "POL", "Porucznik Stanisław Zieliński", "S. ZIELIŃSKI" },
		{ "NED", "Luitenant Jan de Vries", "J. DE VRIES" },
		{ "FIN", "Luutnantti Väinö Mäkelä", "V. MÄKELÄ" },
		{ "HUN", "Főhadnagy László Kovács", "L. KOVÁCS" },
		{ "FRA", "Sergent-chef Jean Arnaud", "J. ARNAUD" },
		{ "ROM", "Căpitan Ion Popescu", "I. POPESCU" },
		{ "CHN", "Captain Wang Wei", "WANG W." },
		{ "GRE", "Squadron Leader Tom Pryce", "S/LDR T. PRYCE" },
	};

	std::string Joined(const std::vector<std::string>& All)
	{
		std::string Out;
		for (const std::string& One : All) Out += (Out.empty() ? "" : " | ") + One;
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	int Bad = 0;
	// The README's examples: "8 German and 4 Italian kills: eight crosses on the first row, four tricolours on the
	// second; 5 German, 3 Italian, 2 British and 1 Soviet: crosses, then tricolours, then two roundels, an empty slot and
	// a star."
	{
		const std::vector<std::string> A = AirTally::KillsInOrder("USA", 12, { { "GER", 8 }, { "ITA", 4 } });
		const bool bA = A.size() == 14 && A[0] == "KILL-GER" && A[7] == "KILL-GER" && A[8].empty() && A[9].empty() && A[10] == "KILL-ITA" && A[13] == "KILL-ITA";
		const std::vector<std::string> B = AirTally::KillsInOrder("USA", 11, { { "GER", 5 }, { "ITA", 3 }, { "UK", 2 }, { "SOV", 1 } });
		const bool bB = B.size() == 24 && B[4] == "KILL-GER" && B[5].empty() && B[10] == "KILL-ITA" && B[12] == "KILL-ITA" && B[20] == "KILL-UK"
			&& B[21] == "KILL-UK" && B[22].empty() && B[23] == "KILL-SOV";
		std::printf("  %s  the README's first example (crosses on the first row, tricolours on the second)\n", bA ? "ok  " : "FAIL");
		std::printf("  %s  the README's second example (crosses, tricolours, two roundels, an empty slot, a star)\n", bB ? "ok  " : "FAIL");
		Bad += (bA ? 0 : 1) + (bB ? 0 : 1);
	}
	int Count = 0, Wrong = 0;
	for (const Case& C : Cases)
	{
		++Count;
		const std::vector<AirTally::Mark> Got = AirTally::Fill(C.Nation, C.Missions, C.Kills, ByOf(C.By), C.MissionSlots, C.KillSlots, C.Seat);
		std::string Why;
		if (!Same(C.Marks, Got, Why))
		{
			++Wrong;
			std::printf("  FAIL  %s %d missions, %d kills (%s), %d and %d slots, seat %d: %s\n    got  %s\n", C.Nation, C.Missions, C.Kills, C.By,
				C.MissionSlots, C.KillSlots, C.Seat, Why.c_str(), Laid(Got).c_str());
		}
	}
	std::printf("  %s  %d of %d records laid out as the modeller's tallies.fill lays them\n", Wrong == 0 ? "ok  " : "FAIL", Count - Wrong, Count);
	Bad += Wrong > 0 ? 1 : 0;

	// A fighter's own marks to a row: the Spitfire's nine (her number covers a tenth column), three rows, 27 at most.
	{
		std::string Why;
		const std::vector<AirTally::Mark> A = AirTally::Fill("USA", 0, 12, { { "GER", 8 }, { "ITA", 4 } }, 0, 27, 0, 9);
		const bool bA = Same("K0:GER:0.850 K1:GER:0.871 K2:GER:0.893 K3:GER:0.914 K4:GER:0.936 K5:GER:0.957 K6:GER:0.979 K7:GER:1.000 "
			"K9:ITA:0.850 K10:ITA:0.900 K11:ITA:0.950 K12:ITA:1.000", A, Why);
		Bad += Check(bA, "nine to a row: eight crosses, the tricolours from the second row's first slot", bA ? "" : Why + "; got " + Laid(A));
		const std::vector<AirTally::Mark> B = AirTally::Fill("UK", 0, 30, { { "GER", 30 } }, 0, 27, 0, 9);
		bool bB = B.size() == 27;
		for (size_t K = 0; bB && K < B.size(); ++K) bB = B[K].Slot == static_cast<int>(K) && B[K].Row == static_cast<int>(K) / 9 && B[K].Tag == "KILL-GER";
		Bad += Check(bB, "nine to a row: 30 kills fill her 27 slots, three rows of nine", bB ? "" : Laid(B));
		const std::vector<AirTally::Mark> C = AirTally::Fill("USA", 0, 12, { { "GER", 8 }, { "ITA", 4 } }, 0, 30, 0);
		const bool bC = Same("K0:GER:0.850 K1:GER:0.871 K2:GER:0.893 K3:GER:0.914 K4:GER:0.936 K5:GER:0.957 K6:GER:0.979 K7:GER:1.000 "
			"K10:ITA:0.850 K11:ITA:0.900 K12:ITA:0.950 K13:ITA:1.000", C, Why);
		Bad += Check(bC, "ten to a row as before where the model says nothing", bC ? "" : Laid(C));
	}

	// His title in the forms that may be painted: rank short as his nation wrote it, initials and surname.
	{
		int Wrong2 = 0;
		for (const Title& T : Titles)
		{
			const std::vector<std::string> Tries = AirTally::NameTries(T.Nation, T.Given);
			if (Tries.empty() || Tries.front() != T.Painted)
			{
				++Wrong2;
				std::printf("  FAIL  %s \"%s\": wants \"%s\", got %s\n", T.Nation, T.Given, T.Painted, Joined(Tries).c_str());
			}
		}
		Bad += Check(Wrong2 == 0, "titles painted as ACES.md writes them", std::to_string(std::size(Titles) - static_cast<size_t>(Wrong2)) + " of " + std::to_string(std::size(Titles)));
		const std::vector<std::string> Tries = AirTally::NameTries("UK", "Flight Lieutenant Tom Harland");
		const bool bOrder = Tries.size() == 3 && Tries[1] == "F/LT HARLAND" && Tries[2] == "HARLAND";
		Bad += Check(bOrder, "shortened in order: rank and surname, then the surname alone", Joined(Tries));
		const std::vector<std::string> Navy = AirTally::NameTries("UK", "Lieutenant Commander Robert Allingham");
		const bool bWhole = Navy.size() == 3 && Navy[1] == "LT CDR ALLINGHAM" && Navy[2] == "ALLINGHAM";
		Bad += Check(bWhole, "a two-word rank kept whole when shortened (the Fleet Air Arm's LT CDR)", Joined(Navy));
		bool bSamples = true;
		for (const char* Nation : { "UK", "USA", "GER", "SOV", "JAP", "ITA", "FRA", "POL", "FIN", "NED", "NOR", "ROM", "HUN", "CHN" })
			bSamples = bSamples && !AirTally::NameTries(Nation, AirTally::SampleTitle(Nation)).empty();
		Bad += Check(bSamples, "every nation's test ace has a name to paint", "");
	}

	// His ink: yellow on an American machine's olive drab, else black on light paint and off-white on dark.
	{
		const bool bInk = AirTally::NameInk(0x4F5236, "USA") == 0xD9B43Au && AirTally::NameInk(0x4C4B35, "USA") == 0xD9B43Au
			&& AirTally::NameInk(0x565C3A, "USA") == 0xD9B43Au && AirTally::NameInk(0x555A47, "USA") == 0xD9B43Au
			&& AirTally::NameInk(0x4F5236, "UK") == 0xF2F0E8u && AirTally::NameInk(0x84878A, "USA") == 0x1A1A1Au
			&& AirTally::NameInk(0x4B5C68, "USA") == 0xF2F0E8u && AirTally::NameInk(0x2A3850, "USA") == 0xF2F0E8u
			&& AirTally::NameInk(0xC0C4C8, "USA") == 0x1A1A1Au && AirTally::NameInk(0x5A6E8C, "USA") == 0xF2F0E8u
			&& AirTally::NameInk(0xD8CFAE, "ITA") == 0x1A1A1Au && AirTally::NameInk(0x2E2E2E, "GER") == 0xF2F0E8u;
		Bad += Check(bInk, "his ink for the paint under his name (the owner decides the olive drab)", "");
	}

	// The letters' sheet: every capital the rules' names use, each cell's ink inside its cell, and every name fitting
	// every fighter's line.
	{
		const std::string Path = Argc > 1 ? Argv[1] : "Content/Data/Tally/name-letters.json";
		std::ifstream In(Path, std::ios::binary);
		const std::string Json((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
		AirTally::Letters Hand;
		const bool bRead = AirTally::ReadLetters(Json, Hand);
		Bad += Check(bRead, "the letters' sheet's index read", Path);
		// (the game's own fighters, Content/Models beside Content/Data: each carries the line laid on here)
		{
			const size_t Data = Path.find("Data/Tally/");
			const std::string Models = Data == std::string::npos ? std::string("Content/Models/") : Path.substr(0, Data) + "Models/";
			int Carry = 0, Absent = 0;
			std::string Off;
			for (const Line& L : Lines)
			{
				std::ifstream File(Models + L.Name + ".json", std::ios::binary);
				if (!File) { ++Absent; continue; }
				const std::string Text((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
				size_t At = Text.find("\"tally\":");
				const size_t Name = At == std::string::npos ? At : Text.find("\"name\":{", At);
				const double Cap = NumberAfter(Text, "cap", Name, At);
				const double CapMin = NumberAfter(Text, "cap_min", Name, At);
				const double Long = NumberAfter(Text, "long", Name, At);
				const size_t Kills = Name == std::string::npos ? Name : Text.find("\"kills\":{", Name);
				const int PerRow = static_cast<int>(NumberAfter(Text, "per_row", Kills, At));
				const size_t Slots = At == std::string::npos ? At : Text.find("\"slots\":[", At);
				const int Held = Slots == std::string::npos ? 0 : SlotsAt(Text, Slots + 8);
				if (Name == std::string::npos || std::abs(Cap - L.Cap) > 1e-9 || std::abs(CapMin - L.CapMin) > 1e-9 || std::abs(Long - L.Long) > 1e-9
					|| PerRow != L.PerRow || Held != L.Slots)
				{
					char Got[160];
					std::snprintf(Got, sizeof(Got), " %s (cap %.3f, least %.3f, long %.2f, %d a row, %d slots)", L.Name, Cap, CapMin, Long, PerRow, Held);
					Off += Got;
				}
				else ++Carry;
			}
			Bad += Check(Off.empty() && Carry >= 14, "the game's own fighters carry the name lines and kill rows laid on here",
				std::to_string(Carry) + " of " + std::to_string(std::size(Lines)) + " (" + std::to_string(Absent) + " not in the game: the what-ifs)" + Off);
		}
		if (bRead)
		{
			std::string Missing;
			for (const char* Ch : { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V",
				"W", "X", "Y", "Z", ".", "/", "(", ")", "-", "Á", "Ä", "É", "Ó", "Ö", "Ø", "Ă", "Ą", "Ł", "Ń", "Ő", "Ż", "Ș", "Ț", "BAR" })
				if (!Hand.Cells.count(Ch)) Missing += std::string(Missing.empty() ? "" : " ") + Ch;
			Bad += Check(Missing.empty(), "a cell for every capital the rules' names use, and the bar", Missing.empty() ? std::to_string(Hand.Cells.size()) + " cells" : "none for " + Missing);
			const std::string Bytes = Path.substr(0, Path.size() - 5) + ".a8";
			std::ifstream Sheet(Bytes, std::ios::binary);
			const std::string Pixels((std::istreambuf_iterator<char>(Sheet)), std::istreambuf_iterator<char>());
			bool bSize = Pixels.size() == static_cast<size_t>(Hand.Width) * static_cast<size_t>(Hand.Height);
			int Empty = 0, Edge = 0;
			for (const auto& [Ch, Cell] : Hand.Cells)
			{
				if (!bSize) break;
				int Ink = 0;
				for (int Y = 0; Y < Hand.CellH; ++Y)
					for (int X = 0; X < Hand.CellW; ++X)
					{
						const unsigned char A = static_cast<unsigned char>(Pixels[static_cast<size_t>((Cell.Row * Hand.CellH + Y) * Hand.Width + Cell.Col * Hand.CellW + X)]);
						if (A == 0) continue;
						++Ink;
						// (a letter's ink kept two pixels in from its cell's edge, so its neighbours never bleed into it)
						if (Ch != "BAR" && (X < 2 || Y < 2 || X >= Hand.CellW - 2 || Y >= Hand.CellH - 2)) ++Edge;
					}
				Empty += Ink == 0 ? 1 : 0;
			}
			Bad += Check(bSize && Empty == 0 && Edge == 0, "the sheet as its index says, every cell inked inside its edge",
				bSize ? std::to_string(Empty) + " empty, " + std::to_string(Edge) + " pixels at an edge" : "the sheet is " + std::to_string(Pixels.size()) + " bytes");

			// Every title on every fighter's line: at her capitals' height or smaller, never under the least unless his
			// surname alone, within her line, left-aligned, letters in order and apart.
			int Fits = 0, Short = 0, Wrong3 = 0;
			std::string First;
			for (const Line& L : Lines)
			{
				for (const Title& T : Titles)
				{
					const AirTally::NameLaid Laid = AirTally::LayName(AirTally::NameTries(T.Nation, T.Given), Hand, L.Long, L.Cap, L.CapMin);
					bool bOk = Laid.Cap <= L.Cap + 1e-12 && Laid.Run <= L.Long + 1e-9 && !Laid.Letters.empty() && Laid.Missing.empty()
						&& Laid.Letters.front().Middle - 0.5 * Laid.Letters.front().Wide >= -1e-9 && Laid.Letters.back().Middle + 0.5 * Laid.Letters.back().Wide <= L.Long + 1e-9;
					for (size_t K = 1; bOk && K < Laid.Letters.size(); ++K)
						bOk = Laid.Letters[K].Middle - Laid.Letters[K - 1].Middle >= 0.5 * (Laid.Letters[K].Wide + Laid.Letters[K - 1].Wide) - 1e-9;
					if (Laid.Cap < L.CapMin - 1e-12 && Laid.Text.find(' ') != std::string::npos) bOk = false;
					Fits += bOk ? 1 : 0;
					Short += Laid.Text != T.Painted ? 1 : 0;
					if (!bOk && ++Wrong3 <= 3)
						First += std::string(First.empty() ? "" : "; ") + L.Name + " " + T.Given + " -> " + Laid.Text + " at " + std::to_string(Laid.Cap);
				}
			}
			Bad += Check(Wrong3 == 0, "every title fits every fighter's line", std::to_string(Fits) + " of " + std::to_string(std::size(Lines) * std::size(Titles))
				+ " (" + std::to_string(Short) + " shortened)" + (First.empty() ? "" : ": " + First));
			// (a long one on the Spitfire's short line: shrunk to the least, then shortened, never under the least but
			// his surname alone)
			const std::vector<std::string> LongTries = AirTally::NameTries("UK", "Wing Commander Christopher Montgomery-Fotheringham-Whitcombe");
			const AirTally::NameLaid Long = AirTally::LayName(LongTries, Hand, 1.06, 0.05, 0.035);
			Bad += Check(LongTries.size() == 3 && Long.Text != LongTries.front() && (Long.Cap >= 0.035 || Long.Text == LongTries.back()),
				"a long name shortened as ACES.md says", Long.Text + " at " + std::to_string(Long.Cap) + " m");
		}
	}
	std::printf(Bad == 0 ? "tallies: all checks pass\n" : "tallies: %d checks FAILED\n", Bad);
	return Bad == 0 ? 0 : 1;
}
