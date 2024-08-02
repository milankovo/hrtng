/*
    Copyright © 2017-2024 AO Kaspersky Lab

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

    Author: Sergey.Belov at kaspersky.com
*/

/*
 * This feature implementation is inspired by existence of "comhelper" plugin included into IDA Pro
 */

#include "warn_off.h"
#include <hexrays.hpp>
#include <diskio.hpp>
#include <typeinf.hpp>
#include "warn_on.h"

#include "helpers.h"
#include "comhelper.h"
#include "rename.h"

#ifdef _MSC_VER
# pragma pack(push,1)
#else
# pragma pack(1)
#endif
#ifdef __GNUC__
# define ATTR_PACKED __attribute__ ((packed))
#else
# define ATTR_PACKED
#endif
struct ida_local guid_t {
	union {
		struct {
			uint32 d1;
			uint16 d2;
			uint16 d3;
			uint16 d4;
			uint8 d5[6];
		} m1;
		struct {
			uint64 lo;
			uint64 hi;
		} m2;
	} u;
	bool fromEa(ea_t ea);
	void print(qstring* str);
	friend bool operator == (const guid_t &id1, const guid_t &id2) {
		return ((id1.u.m2.hi == id2.u.m2.hi) && (id1.u.m2.lo == id2.u.m2.lo));
	}
}  ATTR_PACKED;
#ifdef _MSC_VER
# pragma pack(pop)
#else
# pragma pack()
#endif

bool guid_t::fromEa(ea_t ea)
{
	if (16 != get_bytes(&u.m2, sizeof(guid_t), ea))
		return false;
	return true;
}

void guid_t::print(qstring* str)
{
	str->sprnt("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", u.m1.d1, u.m1.d2, u.m1.d3, (uint8)u.m1.d4, u.m1.d4 >> 8 , u.m1.d5[0], u.m1.d5[1], u.m1.d5[2], u.m1.d5[3], u.m1.d5[4], u.m1.d5[5]);
}

struct ida_local guid_ex_t {
	guid_t uid;
	qstring name;
};

qvector<guid_ex_t> guids;

const char * idaapi read_ioports_cb(const ioports_t &ports, const char *line)
{
	guid_ex_t guid;
	uint32 d5[6];
	char name[1024];
	name[0] = 0;
	if (11 != qsscanf(line, "guid {%x-%hx-%hx-%hx-%2x%2x%2x%2x%2x%2x} %1024s", 
		&guid.uid.u.m1.d1, &guid.uid.u.m1.d2, &guid.uid.u.m1.d3, &guid.uid.u.m1.d4,
		&d5[0], &d5[1], &d5[2], &d5[3], &d5[4], &d5[5], name))
		return "bad guid format";
	for (int i = 0; i < 6; i++)
		guid.uid.u.m1.d5[i] = (uint8)d5[i];
	guid.uid.u.m1.d4 = swap16(guid.uid.u.m1.d4);
	guid.name = name;
	guids.push_back(guid);
	return NULL;
}

static bool bImported = false;
void com_init()
{
	if (guids.size())
		return;
	ioports_t ioports;
	qstring device;
	read_ioports(&ioports, &device, "clsid.cfg", read_ioports_cb);

	if (!bImported) {
		bImported = true;
		if(is64bit())
			add_til("vc10_64", ADDTIL_INCOMP);
		else
			add_til("vc6win", ADDTIL_INCOMP);
		import_type(get_idati(), -1, "IDispatchVtbl", 0);
	}
}

static bool isGUIDtypeName(const char* tname)
{
	if (qstrcmp(tname, "CLSID") == 0 ||
		qstrcmp(tname, "IID") == 0 ||
	  qstrcmp(tname, "GUID") == 0 ||
	  qstrcmp(tname, "EFI_GUID") == 0)
		return true;
	return false;
}

static bool isGUIDtype(const tinfo_t &ti)
{
	qstring typeName;
	if (!ti.get_type_name(&typeName))
		ti.print(&typeName);
	return isGUIDtypeName(typeName.c_str());
}

static tid_t com_find_guid_type(ea_t ea, flags64_t flags, qstring* comment = NULL)
{
	guid_t guid;
	if (!guid.fromEa(ea))
		return BADNODE;

	com_init();

	for (qvector<guid_ex_t>::iterator it = guids.begin(); it != guids.end(); it++) {
		if (it->uid == guid) {
			if (comment)
				*comment = it->name;
			if (!has_user_name(flags)) {
				qstring name("CLSID_");
				name += it->name;
				set_name(ea, name.c_str(), SN_NOCHECK | SN_NON_AUTO | SN_NOWARN | SN_FORCE );
				set_cmt(ea, name.c_str(), true);
				msg("[hrt] %a: clsid '%s' was found\n", ea, name.c_str());
			}
			qstring vtname = it->name;
			vtname += "Vtbl";
			if (BADADDR == get_struc_id(vtname.c_str())) {
				const char* verb = (BADNODE != import_type(get_idati(), -1, vtname.c_str(), 0)) ? "imported" : "not found";
				msg("[hrt] %a: type '%s' was %s\n", ea, vtname.c_str(), verb);
			}

			tid_t res = get_struc_id(it->name.c_str());
			if (BADNODE == res) {
				res = import_type(get_idati(), -1, it->name.c_str(), 0);
				const char* verb = (BADNODE != res) ? "imported" : "not found";
				msg("[hrt] %a: type %s was %s\n", ea, it->name.c_str(), verb);

			}
			return res;
		}
	}
	qstring cmt;
	guid.print(&cmt);
	if (comment)
		*comment = cmt;
	set_cmt(ea, cmt.c_str(), true);
	msg("[hrt] %a: clsid '%s' was not found \n", ea, cmt.c_str());
	return BADNODE;
}

void com_make_data_cb(ea_t ea, flags64_t flags, tid_t tid, asize_t len)
{
	if (!is_struct(flags) || len != 16)
		return;

	qstring tname = get_struc_name(tid);
	if(tname.empty())
		return;

	if (!isGUIDtypeName(tname.c_str()))
		return;

	com_find_guid_type(ea, flags);
}

static bool isCreateInstanceCall(const char* name, size_t *iRiid, size_t *ipObj, size_t *nArgs)
{
	if (!namecmp(name, "CoCreateInstance") ||
		!namecmp(name, "CoGetClassObject")) {
		*iRiid = 3; *ipObj = 4; *nArgs = 5;
		return true;
	}
	if (!namecmp(name, "CreateInstance") ||
        !namecmp(name, "CLRCreateInstance") ||
        !namecmp(name, "QueryInterface")) {
		*iRiid = 1; *ipObj = 2; *nArgs = 3;
		return true;
	}
	//TODO: special case for CoCreateInstanceEx
	return false;
}

struct ida_local com_visitor_t : public ctree_visitor_t
{
	cfunc_t *func;
	qstring funcname;
	bool cmtModified;
	user_cmts_t *cmts;

	com_visitor_t(cfunc_t *cfunc) : ctree_visitor_t(CV_FAST), func(cfunc), cmtModified(false)
	{
		cmts = restore_user_cmts(cfunc->entry_ea);
		if (cmts == NULL)
			cmts = user_cmts_new();
		get_func_name(&funcname, cfunc->entry_ea);
	}

	~com_visitor_t()
	{
		if (cmtModified)
			func->save_user_cmts();
		user_cmts_free(cmts);
	}

bool chkCall(cexpr_t *call, qstring &comment)
{
	if (call->op != cot_call)
		return false;

	carglist_t &args = *call->a;
	if (!args.size())
		return false;

	qstring callProcName;
	if (!getExpName(func, call->x, &callProcName))
		return false;

	size_t  iRiid, ipObj, nArgs;
	bool bCreateInstanceCall = isCreateInstanceCall(callProcName.c_str(), &iRiid, &ipObj, &nArgs) && nArgs == args.size();
	tid_t obj_tid = BADNODE;


	bool has_func_type_data = false;
	func_type_data_t fi;
	ea_t calldstea;
	tinfo_t tif = remove_pointer(getCallInfo(call, &calldstea));
	//default get_func_details() call may cause INTERR 50689
	if (tif.is_decl_func() && tif.get_func_details(&fi, GTD_NO_ARGLOCS) && fi.size() == args.size())
		has_func_type_data = true;


	for(size_t i = 0; i < args.size(); i++) {
		cexpr_t *argRiid = &args[i];
		if (argRiid->op == cot_cast) //ignore typecast
			argRiid = argRiid->x;
		if (argRiid->op == cot_ref) // skip ref
			argRiid = argRiid->x;
		//try getExpType(func, arg);
		if (argRiid->op == cot_obj) {
			ea_t ea = argRiid->obj_ea;
      if (is_mapped(ea)) {
        flags64_t flg = get_flags(ea);
        tinfo_t eaType;
				if (has_ti(ea))
					get_tinfo(&eaType, ea);
				if (is_unknown(flg) && has_func_type_data)
					eaType = remove_pointer(fi[i].type);

				if (isGUIDtype(eaType)) {
					qstring cmt;
					tid_t tid = com_find_guid_type(ea, flg, &cmt);
					if (bCreateInstanceCall && i == iRiid)
						obj_tid = tid;
					if (tid == BADNODE) {
						cmt.insert("unknown type of clsid: ");
						appendComment(comment, cmt, false);
					}
				}
			}
		}
	}

	if (bCreateInstanceCall && obj_tid != BADNODE) {
		cexpr_t *argObj = &args[ipObj];
		if (argObj->op == cot_cast) //ignore typecast
			argObj = argObj->x;
		if (argObj->op == cot_ref) {
			argObj = argObj->x;
		}
		if (argObj->op == cot_var) {
			lvar_t& var = func->get_lvars()->at(argObj->v.idx);
			tinfo_t &oldType = var.type();
			if (oldType.is_unknown() || oldType.is_pvoid() || oldType.is_scalar()) {
				qstring tname = get_struc_name(obj_tid);
				tinfo_t t = make_pointer(create_typedef(tname.c_str()));
				if (var.set_lvar_type(t)) {
					(&args[ipObj])->calc_type(false); //recalc arg type
					qstring typeStr;
					t.print(&typeStr);
					msg("[hrt] %a %s: Var %s was recast to \"%s\"\n", call->ea, funcname.c_str(), var.name.c_str(), typeStr.c_str());
					return true;
				}
			}
		}
	}
	
	return false;
}

virtual int idaapi visit_expr(cexpr_t *expr)
{
	if (expr->op != cot_call)
		return 0;
	qstring comment;
	chkCall(expr, comment);
	cmtModified |= setComment4Exp(func, cmts, expr, comment.c_str());
	return 0;
}
};

void com_scan(cfunc_t *cfunc)
{
	com_visitor_t cv(cfunc);
	cv.apply_to(&cfunc->body, NULL);
	//cfunc->verify(ALLOW_UNUSED_LABELS, false);
}