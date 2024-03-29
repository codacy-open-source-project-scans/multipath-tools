/*
 * Copyright (c) 2018 Benjamin Marzinski, Redhat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "globals.c"
#include "blacklist.h"
#include "test-log.h"
#include "debug.h"

#include "../libmultipath/blacklist.c"

struct udev_device {
	const char *sysname;
	char *property_list[];
};

const char *
__wrap_udev_device_get_sysname(struct udev_device *udev_device)
{
	assert_non_null(udev_device);
	assert_non_null(udev_device->sysname);
	return udev_device->sysname;
}

struct udev_list_entry *
__wrap_udev_device_get_properties_list_entry(struct udev_device *udev_device)
{
	assert_non_null(udev_device);
	if (!*udev_device->property_list)
		return NULL;
	return (struct udev_list_entry *)udev_device->property_list;
}

struct udev_list_entry *
__wrap_udev_list_entry_get_next(struct udev_list_entry *list_entry)
{
	assert_non_null(list_entry);
	if (!*((char **)list_entry + 1))
		return NULL;
	return (struct udev_list_entry *)(((char **)list_entry) + 1);
}

const char *
__wrap_udev_list_entry_get_name(struct udev_list_entry *list_entry)
{
	return *(const char **)list_entry;
}

vector elist_property_default;
vector blist_devnode_default;
vector blist_devnode_sdb;
vector blist_devnode_sdb_inv;
vector blist_all;
vector blist_device_foo_bar;
vector blist_device_foo_inv_bar;
vector blist_device_foo_bar_inv;
vector blist_device_all;
vector blist_wwid_xyzzy;
vector blist_wwid_xyzzy_inv;
vector blist_protocol_fcp;
vector blist_protocol_fcp_inv;
vector blist_property_wwn;
vector blist_property_wwn_inv;

static int setup(void **state)
{
	struct config conf;

	memset(&conf, 0, sizeof(conf));
	conf.blist_devnode = vector_alloc();
	if (!conf.blist_devnode)
		return -1;
	conf.elist_property = vector_alloc();
	if (!conf.elist_property)
		return -1;
	if (setup_default_blist(&conf) != 0)
		return -1;
	elist_property_default = conf.elist_property;
	blist_devnode_default = conf.blist_devnode;

	blist_devnode_sdb = vector_alloc();
	if (!blist_devnode_sdb ||
	    store_ble(blist_devnode_sdb, "sdb", ORIGIN_CONFIG))
		return -1;
	blist_devnode_sdb_inv = vector_alloc();
	if (!blist_devnode_sdb_inv ||
	    store_ble(blist_devnode_sdb_inv, "!sdb", ORIGIN_CONFIG))
		return -1;

	blist_all = vector_alloc();
	if (!blist_all || store_ble(blist_all, ".*", ORIGIN_CONFIG))
		return -1;

	blist_device_foo_bar = vector_alloc();
	if (!blist_device_foo_bar || alloc_ble_device(blist_device_foo_bar) ||
	    set_ble_device(blist_device_foo_bar, "foo", "bar", ORIGIN_CONFIG))
		return -1;
	blist_device_foo_inv_bar = vector_alloc();
	if (!blist_device_foo_inv_bar ||
	    alloc_ble_device(blist_device_foo_inv_bar) ||
	    set_ble_device(blist_device_foo_inv_bar, "!foo", "bar", ORIGIN_CONFIG))
		return -1;
	blist_device_foo_bar_inv = vector_alloc();
	if (!blist_device_foo_bar_inv ||
	    alloc_ble_device(blist_device_foo_bar_inv) ||
	    set_ble_device(blist_device_foo_bar_inv, "foo", "!bar", ORIGIN_CONFIG))
		return -1;

	blist_device_all = vector_alloc();
	if (!blist_device_all || alloc_ble_device(blist_device_all) ||
	    set_ble_device(blist_device_all, ".*", ".*", ORIGIN_CONFIG))
		return -1;

	blist_wwid_xyzzy = vector_alloc();
	if (!blist_wwid_xyzzy ||
	    store_ble(blist_wwid_xyzzy, "xyzzy", ORIGIN_CONFIG))
		return -1;
	blist_wwid_xyzzy_inv = vector_alloc();
	if (!blist_wwid_xyzzy_inv ||
	    store_ble(blist_wwid_xyzzy_inv, "!xyzzy", ORIGIN_CONFIG))
		return -1;

	blist_protocol_fcp = vector_alloc();
	if (!blist_protocol_fcp ||
	    store_ble(blist_protocol_fcp, "scsi:fcp", ORIGIN_CONFIG))
		return -1;
	blist_protocol_fcp_inv = vector_alloc();
	if (!blist_protocol_fcp_inv ||
	    store_ble(blist_protocol_fcp_inv, "!scsi:fcp", ORIGIN_CONFIG))
		return -1;

	blist_property_wwn = vector_alloc();
	if (!blist_property_wwn ||
	    store_ble(blist_property_wwn, "ID_WWN", ORIGIN_CONFIG))
		return -1;
	blist_property_wwn_inv = vector_alloc();
	if (!blist_property_wwn_inv ||
	    store_ble(blist_property_wwn_inv, "!ID_WWN", ORIGIN_CONFIG))
		return -1;

	init_test_verbosity(4);
	return 0;
}

static int teardown(void **state)
{
	free_blacklist(elist_property_default);
	free_blacklist(blist_devnode_default);
	free_blacklist(blist_devnode_sdb);
	free_blacklist(blist_devnode_sdb_inv);
	free_blacklist(blist_all);
	free_blacklist_device(blist_device_foo_bar);
	free_blacklist_device(blist_device_foo_inv_bar);
	free_blacklist_device(blist_device_foo_bar_inv);
	free_blacklist_device(blist_device_all);
	free_blacklist(blist_wwid_xyzzy);
	free_blacklist(blist_wwid_xyzzy_inv);
	free_blacklist(blist_protocol_fcp);
	free_blacklist(blist_protocol_fcp_inv);
	free_blacklist(blist_property_wwn);
	free_blacklist(blist_property_wwn_inv);
	return 0;
}

static int reset_blists(void **state)
{
	conf.blist_devnode = NULL;
	conf.blist_wwid = NULL;
	conf.blist_property = NULL;
	conf.blist_protocol = NULL;
	conf.blist_device = NULL;
	conf.elist_devnode = NULL;
	conf.elist_wwid = NULL;
	conf.elist_property = NULL;
	conf.elist_protocol = NULL;
	conf.elist_device = NULL;
	return 0;
}

static void test_devnode_blacklist(void **state)
{
	expect_condlog(3, "sdb: device node name blacklisted\n");
	assert_int_equal(filter_devnode(blist_devnode_sdb, NULL, "sdb"),
			 MATCH_DEVNODE_BLIST);
	assert_int_equal(filter_devnode(blist_devnode_sdb_inv, NULL, "sdb"),
			 MATCH_NOTHING);
	expect_condlog(3, "sdc: device node name blacklisted\n");
	assert_int_equal(filter_devnode(blist_devnode_sdb_inv, NULL, "sdc"),
			 MATCH_DEVNODE_BLIST);
}

static void test_devnode_whitelist(void **state)
{
	expect_condlog(3, "sdb: device node name whitelisted\n");
	assert_int_equal(filter_devnode(blist_all, blist_devnode_sdb, "sdb"),
			 MATCH_DEVNODE_BLIST_EXCEPT);
	expect_condlog(3, "sdc: device node name blacklisted\n");
	assert_int_equal(filter_devnode(blist_all, blist_devnode_sdb, "sdc"),
			 MATCH_DEVNODE_BLIST);
}

static void test_devnode_missing(void **state)
{
	assert_int_equal(filter_devnode(blist_devnode_sdb, NULL, "sdc"),
			 MATCH_NOTHING);
}

static void test_devnode_default(void **state)
{
	assert_int_equal(filter_devnode(blist_devnode_default, NULL, "sdaa"),
			 MATCH_NOTHING);
	if (nvme_multipath_enabled()) {
		expect_condlog(3, "nvme0n1: device node name blacklisted\n");
		assert_int_equal(filter_devnode(blist_devnode_default, NULL,
						"nvme0n1"),
				 MATCH_DEVNODE_BLIST);
	} else
		assert_int_equal(filter_devnode(blist_devnode_default, NULL,
						"nvme0n1"),
				 MATCH_NOTHING);
	assert_int_equal(filter_devnode(blist_devnode_default, NULL, "dasda"),
			 MATCH_NOTHING);
	expect_condlog(3, "hda: device node name blacklisted\n");
	assert_int_equal(filter_devnode(blist_devnode_default, NULL, "hda"),
			 MATCH_DEVNODE_BLIST);
}

static void test_device_blacklist(void **state)
{
	expect_condlog(3, "sdb: (foo:bar) vendor/product blacklisted\n");
	assert_int_equal(filter_device(blist_device_foo_bar, NULL, "foo",
				       "bar", "sdb"),
			 MATCH_DEVICE_BLIST);
	assert_int_equal(filter_device(blist_device_foo_inv_bar, NULL, "foo",
				        "bar", "sdb"),
			 MATCH_NOTHING);
	assert_int_equal(filter_device(blist_device_foo_bar_inv, NULL, "foo",
				        "bar", "sdb"),
			 MATCH_NOTHING);
	expect_condlog(3, "sdb: (baz:bar) vendor/product blacklisted\n");
	assert_int_equal(filter_device(blist_device_foo_inv_bar, NULL, "baz",
				        "bar", "sdb"),
			 MATCH_DEVICE_BLIST);
	expect_condlog(3, "sdb: (foo:baz) vendor/product blacklisted\n");
	assert_int_equal(filter_device(blist_device_foo_bar_inv, NULL, "foo",
				        "baz", "sdb"),
			 MATCH_DEVICE_BLIST);
}

static void test_device_whitelist(void **state)
{
	expect_condlog(3, "sdb: (foo:bar) vendor/product whitelisted\n");
	assert_int_equal(filter_device(blist_device_all, blist_device_foo_bar,
				       "foo", "bar", "sdb"),
			 MATCH_DEVICE_BLIST_EXCEPT);
	expect_condlog(3, "sdb: (foo:baz) vendor/product blacklisted\n");
	assert_int_equal(filter_device(blist_device_all, blist_device_foo_bar,
				       "foo", "baz", "sdb"),
			 MATCH_DEVICE_BLIST);
}

static void test_device_missing(void **state)
{
	assert_int_equal(filter_device(blist_device_foo_bar, NULL, "foo",
				       "baz", "sdb"),
			 MATCH_NOTHING);
}

static void test_wwid_blacklist(void **state)
{
	expect_condlog(3, "sdb: wwid xyzzy blacklisted\n");
	assert_int_equal(filter_wwid(blist_wwid_xyzzy, NULL, "xyzzy", "sdb"),
			 MATCH_WWID_BLIST);
	assert_int_equal(filter_wwid(blist_wwid_xyzzy_inv, NULL, "xyzzy",
				     "sdb"), MATCH_NOTHING);
	expect_condlog(3, "sdb: wwid plugh blacklisted\n");
	assert_int_equal(filter_wwid(blist_wwid_xyzzy_inv, NULL, "plugh",
				     "sdb"), MATCH_WWID_BLIST);
}

static void test_wwid_whitelist(void **state)
{
	expect_condlog(3, "sdb: wwid xyzzy whitelisted\n");
	assert_int_equal(filter_wwid(blist_all, blist_wwid_xyzzy,
				     "xyzzy", "sdb"),
			 MATCH_WWID_BLIST_EXCEPT);
	expect_condlog(3, "sdb: wwid plugh blacklisted\n");
	assert_int_equal(filter_wwid(blist_all, blist_wwid_xyzzy,
				     "plugh", "sdb"),
			 MATCH_WWID_BLIST);
}

static void test_wwid_missing(void **state)
{
	assert_int_equal(filter_wwid(blist_wwid_xyzzy, NULL, "plugh", "sdb"),
			 MATCH_NOTHING);
}

static void test_protocol_blacklist(void **state)
{
	struct path pp = { .dev = "sdb", .bus = SYSFS_BUS_SCSI,
			   .sg_id.proto_id = SCSI_PROTOCOL_FCP };
	expect_condlog(3, "sdb: protocol scsi:fcp blacklisted\n");
	assert_int_equal(filter_protocol(blist_protocol_fcp, NULL, &pp),
			 MATCH_PROTOCOL_BLIST);
	assert_int_equal(filter_protocol(blist_protocol_fcp_inv, NULL, &pp),
			 MATCH_NOTHING);
	pp.sg_id.proto_id = SCSI_PROTOCOL_ATA;
	expect_condlog(3, "sdb: protocol scsi:ata blacklisted\n");
	assert_int_equal(filter_protocol(blist_protocol_fcp_inv, NULL, &pp),
			 MATCH_PROTOCOL_BLIST);
}

static void test_protocol_whitelist(void **state)
{
	struct path pp1 = { .dev = "sdb", .bus = SYSFS_BUS_SCSI,
			   .sg_id.proto_id = SCSI_PROTOCOL_FCP };
	struct path pp2 = { .dev = "sdb", .bus = SYSFS_BUS_SCSI,
			   .sg_id.proto_id = SCSI_PROTOCOL_ISCSI };
	expect_condlog(3, "sdb: protocol scsi:fcp whitelisted\n");
	assert_int_equal(filter_protocol(blist_all, blist_protocol_fcp, &pp1),
			 MATCH_PROTOCOL_BLIST_EXCEPT);
	expect_condlog(3, "sdb: protocol scsi:iscsi blacklisted\n");
	assert_int_equal(filter_protocol(blist_all, blist_protocol_fcp, &pp2),
			 MATCH_PROTOCOL_BLIST);
}

static void test_protocol_missing(void **state)
{
	struct path pp = { .dev = "sdb", .bus = SYSFS_BUS_SCSI,
			   .sg_id.proto_id = SCSI_PROTOCOL_ISCSI };
	assert_int_equal(filter_protocol(blist_protocol_fcp, NULL, &pp),
			 MATCH_NOTHING);
}

static void test_property_blacklist(void **state)
{
	static struct udev_device udev = { "sdb", { "ID_FOO", "ID_WWN", "ID_BAR", NULL } };
	static struct udev_device udev_inv = { "sdb", { "ID_WWN", NULL } };
	conf.blist_property = blist_property_wwn;
	expect_condlog(3, "sdb: udev property ID_WWN blacklisted\n");
	assert_int_equal(filter_property(&conf, &udev, 3, "ID_SERIAL"),
			 MATCH_PROPERTY_BLIST);
	conf.blist_property = blist_property_wwn_inv;
	expect_condlog(3, "sdb: udev property ID_FOO blacklisted\n");
	assert_int_equal(filter_property(&conf, &udev, 3, "ID_SERIAL"),
			 MATCH_PROPERTY_BLIST);
	assert_int_equal(filter_property(&conf, &udev_inv, 3, "ID_SERIAL"),
			 MATCH_NOTHING);
}

/* the property check works different in that you check all the property
 * names, so setting a blacklist value will blacklist the device if any
 * of the property on the blacklist are found before the property names
 * in the whitelist.  This might be worth changing. although it would
 * force multipath to go through the properties twice */
static void test_property_whitelist(void **state)
{
	static struct udev_device udev = { "sdb", { "ID_FOO", "ID_WWN", "ID_BAR", NULL } };
	conf.elist_property = blist_property_wwn;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	assert_int_equal(filter_property(&conf, &udev, 3, "ID_SERIAL"),
			 MATCH_PROPERTY_BLIST_EXCEPT);
}

static void test_property_missing(void **state)
{
	static struct udev_device udev = { "sdb", { "ID_FOO", "ID_BAZ", "ID_BAR", "ID_SERIAL", NULL } };
	conf.blist_property = blist_property_wwn;
	expect_condlog(3, "sdb: blacklisted, udev property missing\n");
	assert_int_equal(filter_property(&conf, &udev, 3, "ID_SERIAL"),
			 MATCH_PROPERTY_BLIST_MISSING);
	assert_int_equal(filter_property(&conf, &udev, 3, "ID_BLAH"),
			 MATCH_NOTHING);
	assert_int_equal(filter_property(&conf, &udev, 3, ""),
			 MATCH_NOTHING);
	assert_int_equal(filter_property(&conf, &udev, 3, NULL),
			 MATCH_NOTHING);
}

struct udev_device test_udev = { "sdb", { "ID_FOO", "ID_WWN", "ID_BAR", NULL } };

struct path test_pp = { .dev = "sdb", .bus = SYSFS_BUS_SCSI, .udev = &test_udev,
			.sg_id.proto_id = SCSI_PROTOCOL_FCP, .vendor_id = "foo",
			.product_id = "bar", .wwid = "xyzzy" };

static void test_filter_path_property(void **state)
{
	conf.blist_property = blist_property_wwn;
	expect_condlog(3, "sdb: udev property ID_WWN blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_PROPERTY_BLIST);
}

static void test_filter_path_devnode(void **state)
{
	/* always must include property elist, to avoid "missing property"
	 * blacklisting */
	conf.elist_property = blist_property_wwn;
	conf.blist_devnode = blist_devnode_sdb;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	expect_condlog(3, "sdb: device node name blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_DEVNODE_BLIST);
}

static void test_filter_path_device(void **state)
{
	/* always must include property elist, to avoid "missing property"
	 * blacklisting */
	conf.elist_property = blist_property_wwn;
	conf.blist_device = blist_device_foo_bar;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	expect_condlog(3, "sdb: (foo:bar) vendor/product blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_DEVICE_BLIST);
}

static void test_filter_path_protocol(void **state)
{
	conf.elist_property = blist_property_wwn;
	conf.blist_protocol = blist_protocol_fcp;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	expect_condlog(3, "sdb: protocol scsi:fcp blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_PROTOCOL_BLIST);
}

static void test_filter_path_wwid(void **state)
{
	conf.elist_property = blist_property_wwn;
	conf.blist_wwid = blist_wwid_xyzzy;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	expect_condlog(3, "sdb: wwid xyzzy blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_WWID_BLIST);
}

struct udev_device miss_udev = { "sdb", { "ID_FOO", "ID_BAZ", "ID_BAR", "ID_SERIAL", NULL } };

struct path miss1_pp = { .dev = "sdc", .bus = SYSFS_BUS_SCSI,
			.udev = &miss_udev,
			 .uid_attribute = "ID_SERIAL",
			.sg_id.proto_id = SCSI_PROTOCOL_ISCSI,
			.vendor_id = "foo", .product_id = "baz",
			.wwid = "plugh" };

struct path miss2_pp = { .dev = "sdc", .bus = SYSFS_BUS_SCSI,
			.udev = &test_udev,
			 .uid_attribute = "ID_SERIAL",
			.sg_id.proto_id = SCSI_PROTOCOL_ISCSI,
			.vendor_id = "foo", .product_id = "baz",
			.wwid = "plugh" };

struct path miss3_pp = { .dev = "sdc", .bus = SYSFS_BUS_SCSI,
			.udev = &miss_udev,
			 .uid_attribute = "ID_EGGS",
			.sg_id.proto_id = SCSI_PROTOCOL_ISCSI,
			.vendor_id = "foo", .product_id = "baz",
			.wwid = "plugh" };

static void test_filter_path_missing1(void **state)
{
	conf.blist_property = blist_property_wwn;
	conf.blist_devnode = blist_devnode_sdb;
	conf.blist_device = blist_device_foo_bar;
	conf.blist_protocol = blist_protocol_fcp;
	conf.blist_wwid = blist_wwid_xyzzy;
	expect_condlog(3, "sdb: blacklisted, udev property missing\n");
	assert_int_equal(filter_path(&conf, &miss1_pp),
			 MATCH_PROPERTY_BLIST_MISSING);
}

/* This one matches the property whitelist, to test the other missing
 * functions */
static void test_filter_path_missing2(void **state)
{
	conf.elist_property = blist_property_wwn;
	conf.blist_devnode = blist_devnode_sdb;
	conf.blist_device = blist_device_foo_bar;
	conf.blist_protocol = blist_protocol_fcp;
	conf.blist_wwid = blist_wwid_xyzzy;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	assert_int_equal(filter_path(&conf, &miss2_pp),
			 MATCH_NOTHING);
}

/* Here we use a different uid_attribute which is also missing, thus
   the path is not blacklisted */
static void test_filter_path_missing3(void **state)
{
	conf.blist_property = blist_property_wwn;
	conf.blist_devnode = blist_devnode_sdb;
	conf.blist_device = blist_device_foo_bar;
	conf.blist_protocol = blist_protocol_fcp;
	conf.blist_wwid = blist_wwid_xyzzy;
	assert_int_equal(filter_path(&conf, &miss3_pp),
			 MATCH_NOTHING);
}

static void test_filter_path_whitelist(void **state)
{
	conf.elist_property = blist_property_wwn;
	conf.elist_devnode = blist_devnode_sdb;
	conf.elist_device = blist_device_foo_bar;
	conf.elist_protocol = blist_protocol_fcp;
	conf.elist_wwid = blist_wwid_xyzzy;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	expect_condlog(3, "sdb: device node name whitelisted\n");
	expect_condlog(3, "sdb: (foo:bar) vendor/product whitelisted\n");
	expect_condlog(3, "sdb: protocol scsi:fcp whitelisted\n");
	expect_condlog(3, "sdb: wwid xyzzy whitelisted\n");
	assert_int_equal(filter_path(&conf, &test_pp),
			 MATCH_WWID_BLIST_EXCEPT);
}

static void test_filter_path_whitelist_property(void **state)
{
	conf.blist_property = blist_property_wwn;
	conf.elist_devnode = blist_devnode_sdb;
	conf.elist_device = blist_device_foo_bar;
	conf.elist_protocol = blist_protocol_fcp;
	conf.elist_wwid = blist_wwid_xyzzy;
	expect_condlog(3, "sdb: udev property ID_WWN blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_PROPERTY_BLIST);
}

static void test_filter_path_whitelist_devnode(void **state)
{
	conf.elist_property = blist_property_wwn;
	conf.blist_devnode = blist_devnode_sdb;
	conf.elist_device = blist_device_foo_bar;
	conf.elist_protocol = blist_protocol_fcp;
	conf.elist_wwid = blist_wwid_xyzzy;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	expect_condlog(3, "sdb: device node name blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_DEVNODE_BLIST);
}

static void test_filter_path_whitelist_device(void **state)
{
	conf.elist_property = blist_property_wwn;
	conf.elist_devnode = blist_devnode_sdb;
	conf.blist_device = blist_device_foo_bar;
	conf.elist_protocol = blist_protocol_fcp;
	conf.elist_wwid = blist_wwid_xyzzy;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	expect_condlog(3, "sdb: device node name whitelisted\n");
	expect_condlog(3, "sdb: (foo:bar) vendor/product blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_DEVICE_BLIST);
}

static void test_filter_path_whitelist_protocol(void **state)
{
	conf.elist_property = blist_property_wwn;
	conf.elist_devnode = blist_devnode_sdb;
	conf.elist_device = blist_device_foo_bar;
	conf.blist_protocol = blist_protocol_fcp;
	conf.elist_wwid = blist_wwid_xyzzy;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	expect_condlog(3, "sdb: device node name whitelisted\n");
	expect_condlog(3, "sdb: (foo:bar) vendor/product whitelisted\n");
	expect_condlog(3, "sdb: protocol scsi:fcp blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_PROTOCOL_BLIST);
}

static void test_filter_path_whitelist_wwid(void **state)
{
	conf.elist_property = blist_property_wwn;
	conf.elist_devnode = blist_devnode_sdb;
	conf.elist_device = blist_device_foo_bar;
	conf.elist_protocol = blist_protocol_fcp;
	conf.blist_wwid = blist_wwid_xyzzy;
	expect_condlog(3, "sdb: udev property ID_WWN whitelisted\n");
	expect_condlog(3, "sdb: device node name whitelisted\n");
	expect_condlog(3, "sdb: (foo:bar) vendor/product whitelisted\n");
	expect_condlog(3, "sdb: protocol scsi:fcp whitelisted\n");
	expect_condlog(3, "sdb: wwid xyzzy blacklisted\n");
	assert_int_equal(filter_path(&conf, &test_pp), MATCH_WWID_BLIST);
}

#define test_and_reset(x) cmocka_unit_test_teardown((x), reset_blists)

int test_blacklist(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_devnode_blacklist),
		cmocka_unit_test(test_devnode_whitelist),
		cmocka_unit_test(test_devnode_missing),
		cmocka_unit_test(test_devnode_default),
		cmocka_unit_test(test_device_blacklist),
		cmocka_unit_test(test_device_whitelist),
		cmocka_unit_test(test_device_missing),
		cmocka_unit_test(test_wwid_blacklist),
		cmocka_unit_test(test_wwid_whitelist),
		cmocka_unit_test(test_wwid_missing),
		cmocka_unit_test(test_protocol_blacklist),
		cmocka_unit_test(test_protocol_whitelist),
		cmocka_unit_test(test_protocol_missing),
		test_and_reset(test_property_blacklist),
		test_and_reset(test_property_whitelist),
		test_and_reset(test_property_missing),
		test_and_reset(test_filter_path_property),
		test_and_reset(test_filter_path_devnode),
		test_and_reset(test_filter_path_device),
		test_and_reset(test_filter_path_protocol),
		test_and_reset(test_filter_path_wwid),
		test_and_reset(test_filter_path_missing1),
		test_and_reset(test_filter_path_missing2),
		test_and_reset(test_filter_path_missing3),
		test_and_reset(test_filter_path_whitelist),
		test_and_reset(test_filter_path_whitelist_property),
		test_and_reset(test_filter_path_whitelist_devnode),
		test_and_reset(test_filter_path_whitelist_device),
		test_and_reset(test_filter_path_whitelist_protocol),
		test_and_reset(test_filter_path_whitelist_wwid),
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}

int main(void)
{
	int ret = 0;
	ret += test_blacklist();
	return ret;
}
