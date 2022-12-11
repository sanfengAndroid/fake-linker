# -*- coding: utf-8 -*-
import argparse
import logging
import random
import sys
import time
from enum import Enum
from enum import unique
from pathlib import Path

import requests


@unique
class DeviceBrand(Enum):
    Samsung = 'Samsung'
    Google = 'Google'
    OnePlus = 'OnePlus'
    Xiaomi = 'Xiaomi'
    Vivo = 'Vivo'
    Oppo = 'Oppo'
    Motorola = 'Motorola'
    Huawei = 'Huawei'
    SansungTablet = 'Samsung Galaxy Tab'

    @staticmethod
    def to_brand(brand_str: str):
        for brand in DeviceBrand:
            if brand_str == brand.value:
                return brand
        raise Exception(f'Unknown brand {brand_str}')


@unique
class TestState(Enum):
    Running = 'running'
    Failed = 'failed'
    Error = 'error'
    TimedOut = 'timed out'
    Passed = 'passed'
    Queued = 'queued'
    Skipped = 'skipped'

    def to_state(state_str: str):
        if not state_str:
            return TestState.Running
        state_str = state_str.lower()
        for state in TestState:
            if state.value == state_str:
                return state
        raise Exception(f'Unknown test state {state_str}')

    def is_complete(self):
        if self == TestState.Failed:
            return True
        if self == TestState.Error:
            return True
        if self == TestState.TimedOut:
            return True
        if self == TestState.Passed:
            return True
        if self == TestState.Skipped:
            return True
        return False

    def is_success(self):
        if self == TestState.Passed:
            return True
        if self == TestState.Skipped:
            return True
        return False


API_MAP = {
    '13.0': 33,
    '12.0': 31,
    '11.0': 30,
    '10.0': 29,
    '9.0': 28,
    '8.1': 27,
    '8.0': 26,
    '7.1': 25,
    '7.1.1': 25,
    '7.0': 24,
    '6.0': 23,
    '5.1': 22,
    '5.0': 21
}

MAX_PARALLEL = 1
BUILD_QUERY_INTERVAL = 15
TEST_PROJECT = 'fakelinker'
DEVICE_LOG_ENABLE = True


def api_string_to_level(api_str) -> int:
    return API_MAP.get(api_str, 0)


class DeviceInfo:
    def __init__(self, device: str) -> None:
        strs = device.split(' ')
        self.brand = DeviceBrand.to_brand(strs[0])
        self.name = device
        self.api = api_string_to_level(strs[-1].split('-')[-1])

    def is_device(self, name, version):
        return self.name == f'{name}-{version}'

    def __eq__(self, other):
        if isinstance(other, DeviceInfo):
            return self.name == other.name
        return False

    def __hash__(self):
        return self.name.__hash__()


class BrowserStackDevice:
    default_device = DeviceInfo('Google Pixel 7 Pro-13.0')

    def __init__(self) -> None:
        self.android_13 = []
        self.android_13.append(DeviceInfo('Google Pixel 7 Pro-13.0'))
        self.android_13.append(DeviceInfo('Google Pixel 7-13.0'))
        self.android_13.append(DeviceInfo('Google Pixel 6 Pro-13.0'))

        self.android_12 = []
        self.android_12.append(DeviceInfo('Samsung Galaxy S22 Ultra-12.0'))
        self.android_12.append(DeviceInfo('Samsung Galaxy S22 Plus-12.0'))
        self.android_12.append(DeviceInfo('Samsung Galaxy S22-12.0'))
        self.android_12.append(DeviceInfo('Samsung Galaxy S21-12.0'))
        self.android_12.append(DeviceInfo('Google Pixel 6 Pro-12.0'))
        self.android_12.append(DeviceInfo('Google Pixel 6-12.0'))
        self.android_12.append(DeviceInfo('Google Pixel 5-12.0'))
        self.android_12.append(DeviceInfo('Samsung Galaxy Tab S8-12.0'))

        self.android_11 = []
        self.android_11.append(DeviceInfo('Samsung Galaxy S21 Ultra-11.0'))
        self.android_11.append(DeviceInfo('Samsung Galaxy S21-11.0'))
        self.android_11.append(DeviceInfo('Samsung Galaxy S21 Plus-11.0'))
        self.android_11.append(DeviceInfo('Samsung Galaxy M52-11.0'))
        self.android_11.append(DeviceInfo('Samsung Galaxy M32-11.0'))
        self.android_11.append(DeviceInfo('Samsung Galaxy A52-11.0'))
        self.android_11.append(DeviceInfo('Google Pixel 5-11.0'))
        self.android_11.append(DeviceInfo('Google Pixel 4-11.0'))
        self.android_11.append(DeviceInfo('Xiaomi Redmi Note 11-11.0'))
        self.android_11.append(DeviceInfo('Vivo Y21-11.0'))
        self.android_11.append(DeviceInfo('Vivo V21-11.0'))
        self.android_11.append(DeviceInfo('Oppo Reno 6-11.0'))
        self.android_11.append(DeviceInfo('Oppo A96-11.0'))
        self.android_11.append(DeviceInfo('Motorola Moto G71 5G-11.0'))
        self.android_11.append(DeviceInfo('Samsung Galaxy Tab S7-11.0'))

        self.android_10 = []
        self.android_10.append(DeviceInfo('Samsung Galaxy S20-10.0'))
        self.android_10.append(DeviceInfo('Samsung Galaxy S20 Plus-10.0'))
        self.android_10.append(DeviceInfo('Samsung Galaxy S20 Ultra-10.0'))
        self.android_10.append(DeviceInfo('Samsung Galaxy Note 20 Ultra-10.0'))
        self.android_10.append(DeviceInfo('Samsung Galaxy Note 20-10.0'))
        self.android_10.append(DeviceInfo('Samsung Galaxy A51-10.0'))
        self.android_10.append(DeviceInfo('Samsung Galaxy A11-10.0'))
        self.android_10.append(DeviceInfo('Google Pixel 4 XL-10.0'))
        self.android_10.append(DeviceInfo('Google Pixel 4-10.0'))
        self.android_10.append(DeviceInfo('Google Pixel 3-10.0'))
        self.android_10.append(DeviceInfo('OnePlus 8-10.0'))
        self.android_10.append(DeviceInfo('OnePlus 7T-10.0'))
        self.android_10.append(DeviceInfo('Xiaomi Redmi Note 9-10.0'))
        self.android_10.append(DeviceInfo('Oppo Reno 3 Pro-10.0'))
        self.android_10.append(DeviceInfo('Motorola Moto G9 Play-10.0'))
        self.android_10.append(DeviceInfo('Samsung Galaxy Tab S7-10.0'))

        self.android_9 = []
        self.android_9.append(DeviceInfo('Samsung Galaxy S9 Plus-9.0'))
        self.android_9.append(DeviceInfo('Samsung Galaxy S10e-9.0'))
        self.android_9.append(DeviceInfo('Samsung Galaxy S10 Plus-9.0'))
        self.android_9.append(DeviceInfo('Samsung Galaxy S10-9.0'))
        self.android_9.append(DeviceInfo('Samsung Galaxy Note 10 Plus-9.0'))
        self.android_9.append(DeviceInfo('Samsung Galaxy Note 10-9.0'))
        self.android_9.append(DeviceInfo('Samsung Galaxy A10-9.0'))
        self.android_9.append(DeviceInfo('Google Pixel 3a XL-9.0'))
        self.android_9.append(DeviceInfo('Google Pixel 3a-9.0'))
        self.android_9.append(DeviceInfo('Google Pixel 3 XL-9.0'))
        self.android_9.append(DeviceInfo('Google Pixel 3-9.0'))
        self.android_9.append(DeviceInfo('Google Pixel 2-9.0'))
        self.android_9.append(DeviceInfo('OnePlus 7-9.0'))
        self.android_9.append(DeviceInfo('OnePlus 6T-9.0'))
        self.android_9.append(DeviceInfo('Xiaomi Redmi Note 8-9.0'))
        self.android_9.append(DeviceInfo('Xiaomi Redmi Note 7-9.0'))
        self.android_9.append(DeviceInfo('Motorola Moto G7 Play-9.0'))
        self.android_9.append(DeviceInfo('Huawei P30-9.0'))
        self.android_9.append(DeviceInfo('Samsung Galaxy Tab S6-9.0'))
        self.android_9.append(DeviceInfo('Samsung Galaxy Tab S5e-9.0'))

        self.android_8_1 = []
        self.android_8_1.append(DeviceInfo('Samsung Galaxy Note 9-8.1'))
        self.android_8_1.append(DeviceInfo('Samsung Galaxy J7 Prime-8.1'))
        self.android_8_1.append(DeviceInfo('Samsung Galaxy Tab S4-8.1'))

        self.android_8_0 = []
        self.android_8_0.append(DeviceInfo('Samsung Galaxy S9 Plus-8.0'))
        self.android_8_0.append(DeviceInfo('Samsung Galaxy S9-8.0'))
        self.android_8_0.append(DeviceInfo('Google Pixel 2-8.0'))

        self.android_7_1 = []
        self.android_7_1.append(DeviceInfo('Samsung Galaxy Note 8-7.1'))
        self.android_7_1.append(DeviceInfo('Samsung Galaxy A8-7.1'))
        self.android_7_1.append(DeviceInfo('Google Pixel-7.1'))

        self.android_7_0 = []
        self.android_7_0.append(DeviceInfo('Samsung Galaxy S8 Plus-7.0'))
        self.android_7_0.append(DeviceInfo('Samsung Galaxy S8-7.0'))

        self.android_6 = []
        self.android_6.append(DeviceInfo('Samsung Galaxy S7-6.0'))
        self.android_6.append(DeviceInfo('Google Nexus 6-6.0'))

        self.android_5_1 = []

        self.android_5_0 = []
        self.android_5_0.append(DeviceInfo('Samsung Galaxy S6-5.0'))

        self.devices = {
            33: self.android_13,
            32: self.android_12,
            31: self.android_12,
            30: self.android_11,
            29: self.android_10,
            28: self.android_9,
            27: self.android_8_1,
            26: self.android_8_0,
            25: self.android_7_1,
            24: self.android_7_0,
            23: self.android_6,
            22: self.android_5_1,
            21: self.android_5_0
        }

    def random_device(self, api_level=33):
        devices = self.devices.get(api_level, self.android_13)
        return random.choice(devices)

    def random_devices(self, apis: list[int] = [33]):
        result = set()
        for level in apis:
            devices = self.devices.get(level)
            if devices:
                result.add(random.choice(devices))

        return list(result)

    def find_device(self, info: DeviceInfo) -> DeviceInfo:
        devices: list[DeviceInfo] = self.devices.get(info.api, [])
        if devices.count(info) != 0:
            return info
        return None


class BrowserStackResponse:
    def __init__(self, res: requests.Response) -> None:
        self.res = res
        if not res.encoding:
            res.encoding = 'utf-8'
        try:
            self.value: dict = res.json()
        except requests.exceptions.JSONDecodeError as e:
            logging.error('The request returns a non-json format %s', e)
            self.value = res.content
        if not res.ok:
            logging.error('request error: %s', self.content)
            raise requests.exceptions.RequestException(
                f'The request returned an error code: {res.status_code}')

        logging.debug('request url:%s, response:\n%s', res.url, self.value)

    def parse_value(self, name: str, default_value=None):
        return self.value.get(name, default_value)


class BrowserStackBean:
    def __init__(self, res: BrowserStackResponse | dict) -> None:
        if isinstance(res, BrowserStackResponse):
            self.data = res.value
        else:
            self.data = res

    def parse_value(self, name: str, default_value=None):
        return self.data.get(name, default_value)

    def parse_time(self, name: str):
        value = self.parse_value(name)
        if value:
            try:
                s_time = time.strptime(value, '%Y-%m-%d %H:%M:%S %Z')
            except ValueError:
                s_time = time.strptime(value, '%Y-%m-%d %H:%M:%S %z')
            return int(time.mktime(s_time))
        return int(time.time())

    def parse_state(self, name: str):
        val = self.parse_value(name)
        if not val:
            return TestState.Running
        return TestState.to_state(val)

    def parse_int(self, name: str, default_value=-1):
        val = self.parse_value(name, default_value)
        if not val:
            return default_value
        return int(val)


class AppBean(BrowserStackBean):
    def __init__(self, res) -> None:
        super().__init__(res)
        self.app_name = self.parse_value('app_name')
        self.app_url = self.parse_value('app_url')
        self.app_version = self.parse_value('app_version')
        self.app_id = self.parse_value('app_id')
        self.uploaded_at = self.parse_time('uploaded_at')
        self.custom_id = self.parse_value('custom_id')
        self.shareable_id = self.parse_value('shareable_id')
        self.expiry = self.parse_time('expiry')

    def get_app_url(self):
        if self.custom_id:
            return self.custom_id
        if self.shareable_id:
            return self.shareable_id
        return self.app_url


class TestSuiteBean(BrowserStackBean):
    def __init__(self, res) -> None:
        super().__init__(res)
        self.test_suite_name = self.parse_value('test_suite_name')
        self.test_suite_url = self.parse_value('test_suite_url')
        self.test_suite_id = self.parse_value('test_suite_id')
        self.uploaded_at = self.parse_time('uploaded_at')
        self.custom_id = self.parse_value('custom_id')
        self.shareable_id = self.parse_value('shareable_id')
        self.framework = self.parse_value('framework')
        self.expiry = self.parse_time('expiry')

    def get_test_suite_url(self):
        if self.custom_id:
            return self.custom_id
        if self.shareable_id:
            return self.shareable_id
        return self.test_suite_url


class SessionBean(BrowserStackBean):
    def __init__(self, res) -> None:
        super().__init__(res)
        self.id = self.parse_value('id')
        self.status = self.parse_state('status')
        self.start_time = self.parse_time('start_time')
        self.duration = self.parse_int('duration')
        self.testcases = self.parse_value('testcases')

    def is_success(self):
        return self.status.is_success()


class BuildBean(BrowserStackBean):
    def __init__(self, res) -> None:
        super().__init__(res)
        self.build_id = self.parse_value('build_id')
        self.message = self.parse_value('message')
        if not self.build_id:
            self.id = self.parse_value('id')
            self.start_time = self.parse_value('start_time')

            self.framework = self.parse_value('framework')
            if self.framework:
                self.duration = self.parse_int('duration')
                self.status = self.parse_state('status')
                self.input_capabilities = self.parse_value(
                    'input_capabilities')
                self.start_time = self.parse_time('start_time')
                self.app_details = self.parse_value('app_details')
                self.test_suite_details = self.parse_value(
                    'test_suite_details')
                self.devices: list = self.parse_value('devices')

    def parse_session(self, device: DeviceInfo = None) -> list[SessionBean]:
        if not self.devices or len(self.devices) == 0:
            return []
        device_info: dict = None
        for info in self.devices:
            if not device or device.is_device(info.get('device'), info.get('os_version')):
                device_info = info
                break
        if not device_info:
            return []
        return [SessionBean(session) for session in device_info.get('sessions')]

    def parse_failed_device(self) -> list[DeviceInfo]:
        result = []
        for info in self.devices:
            name = info.get('device')
            version = info.get('os_version')
            for session_info in info.get('sessions'):
                if not SessionBean(session_info).is_success():
                    result.append(DeviceInfo(f'{name}-{version}'))
                    break
        return result


class BrowserStack:
    _cloud_api_url = 'https://api-cloud.browserstack.com/'

    def __init__(self, auth: str) -> None:
        ss = auth.split(':')
        self.user = ss[0]
        self.password = ss[1]

    def post(self, url: str, files: dict = None, json_data: dict = None, headers: dict = None):
        res = requests.post(BrowserStack._cloud_api_url + url, auth=(self.user, self.password),
                            json=json_data, headers=headers, files=files)
        logging.debug(
            f'post request url: {url}\n\tfiles: {files}\n\tjson content: {json_data}\n\theaders:{headers}')
        return BrowserStackResponse(res)

    def get(self, url: str, app_id: str = '', test_suite_id: str = '', build_id: str = '', session_id: str = ''):
        url = BrowserStack._cloud_api_url + url
        url = url.format(app_id=app_id, test_suite_id=test_suite_id,
                         build_id=build_id, session_id=session_id)
        logging.debug(f'get request url:{url}')
        return BrowserStackResponse(requests.get(url, auth=(self.user, self.password)))

    def delete(self, url: str, app_id: str = '', test_suite_id: str = '', build_id: str = '', session_id: str = ''):
        url = BrowserStack._cloud_api_url + url
        url = url.format(app_id=app_id, test_suite_id=test_suite_id,
                         build_id=build_id, session_id=session_id)
        logging.debug(f'delete request url:{url}')
        return BrowserStackResponse(requests.delete(url, auth=(self.user, self.password)))


class BrowserStackApp(BrowserStack):
    # POST
    _upload_app_url = 'app-automate/espresso/v2/app'
    # GET
    _list_upload_app_url = 'app-automate/espresso/v2/apps'
    # GET
    _app_detail_get_url = 'app-automate/espresso/v2/apps/{app_id}'
    # DELETE
    _delete_app_url = 'app-automate/espresso/v2/apps/{app_id}'

    def __init__(self, auth: str, app_id=None, custom_id=None) -> None:
        super().__init__(auth)
        self.app_id = app_id
        self.custom_id = custom_id

    def _get_app_id(self, aid):
        app_id = aid if aid else self.app_id
        if isinstance(app_id, AppBean):
            return app_id.app_id
        return app_id

    def _get_custom_id(self, custom_id):
        id = custom_id if custom_id else self.custom_id
        if isinstance(id, AppBean):
            return id.custom_id
        return id

    def update_app(self, file: Path, custom_id: AppBean | str = None) -> AppBean:
        if not file.is_file():
            raise FileExistsError(f'update file not exist {file}')
        files = {
            'file': file.open('rb')
        }
        custom_id = self._get_custom_id(custom_id)
        if custom_id:
            files['custom_id'] = (None, custom_id)
        return AppBean(self.post(self._upload_app_url, files=files))

    def list_upload_app(self) -> list[AppBean]:
        res = self.get(self._list_upload_app_url)
        return [AppBean(data) for data in res.parse_value('apps')]

    def get_last_uplad_app(self) -> AppBean:
        apps = self.list_upload_app()
        if apps:
            return apps[0]
        return None

    def find_custom_id_app(self, custom_id: str) -> AppBean:
        result = None
        for app in self.list_upload_app():
            if app.custom_id == custom_id:
                if not result or app.uploaded_at > result.uploaded_at:
                    result = app
        return app

    def get_app_details(self, app_id: AppBean | str = None) -> AppBean:
        res = self.get(self._app_detail_get_url,
                       app_id=self._get_app_id(app_id))
        return AppBean(res.parse_value('app'))

    def delete_app(self, app_id: AppBean | str = None) -> bool:
        try:
            res = self.delete(self._delete_app_url,
                              app_id=self._get_app_id(app_id))
            return res.parse_value('success') != None
        except requests.exceptions.RequestException as e:
            logging.error('Delete app failed app_id: %s, error: %s', app_id, e)
            return False

    def delete_recent_app(self):
        for app in self.list_upload_app():
            logging.info(f'delete recent app %s result: %s',
                         app.app_id, self.delete_app(app))


class BrowserStackTestSuite(BrowserStack):
    # POST
    _upload_test_suite_url = 'app-automate/espresso/v2/test-suite'
    # GET
    _list_test_suites_url = 'app-automate/espresso/v2/test-suites'
    # GET
    _test_suite_get_url = 'app-automate/espresso/v2/test-suites/{test_suite_id}'
    # DELETE
    _delete_test_suite_url = 'app-automate/espresso/v2/test-suites/{test_suite_id}'

    def __init__(self, auth: str, test_suite_id=None, custom_id=None) -> None:
        super().__init__(auth)
        self.test_suite_id = test_suite_id
        self.custom_id = custom_id

    def _get_test_suite_id(self, tid):
        suite = tid if tid else self.test_suite_id
        if isinstance(suite, TestSuiteBean):
            return suite.test_suite_id
        return suite

    def _get_custom_id(self, custom_id):
        id = custom_id if custom_id else self.custom_id
        if isinstance(id, TestSuiteBean):
            return id.custom_id
        return id

    def upload_test_suite(self, file: Path, custom_id: TestSuiteBean | str = None):
        if not file.is_file():
            raise FileExistsError(f'update file not exist {file}')
        files = {
            'file': file.open('rb')
        }
        custom_id = self._get_custom_id(custom_id)
        if custom_id:
            files['custom_id'] = (None, custom_id)
        return TestSuiteBean(self.post(self._upload_test_suite_url, files=files))

    def list_test_suites(self) -> list[TestSuiteBean]:
        res = self.get(self._list_test_suites_url)
        return [TestSuiteBean(suite) for suite in res.parse_value('test_suites')]

    def get_last_test_suite(self) -> TestSuiteBean:
        suites = self.list_test_suites()
        if len(suites) > 0:
            return suites[0]
        return None

    def find_custom_id_test_suite(self, custom_id) -> TestSuiteBean:
        result = None
        for suite in self.list_test_suites():
            if suite.custom_id == custom_id:
                if not result or suite.uploaded_at > result.uploaded_at:
                    result = suite
        return result

    def get_test_suite_details(self, test_suite_id: TestSuiteBean | str = None) -> TestSuiteBean:
        res = self.get(self._test_suite_get_url,
                       test_suite_id=self._get_test_suite_id(test_suite_id))
        return TestSuiteBean(res.parse_value('test_suite'))

    def delete_test_suite(self, test_suite_id: TestSuiteBean | str = None) -> bool:
        try:
            res = self.delete(self._delete_test_suite_url,
                              test_suite_id=self._get_test_suite_id(test_suite_id))
            return res.parse_value('success') != None
        except requests.exceptions.RequestException as e:
            logging.error(
                'Delete test suite failed id: %s, error: %s', test_suite_id, e)
            return False

    def delete_recent_test_suite(self):
        for suite in self.list_test_suites():
            logging.info('delete test suite %s result %s',
                         suite.test_suite_id, self.delete_test_suite(suite))


class BrowserStackBuild(BrowserStack):
    # POST
    _espresso_build_url = 'app-automate/espresso/v2/build'
    # GET
    _build_state_get_url = 'app-automate/espresso/v2/builds/{build_id}'
    # GET
    _list_recent_builds_url = 'app-automate/espresso/v2/builds'

    def __init__(self, auth: str, build_id=None, app_url=None, test_suite_url=None, devices: list[DeviceInfo] = []) -> None:
        super().__init__(auth)
        self.build_id = build_id
        self.test_suite_url = test_suite_url
        self.app_url = app_url
        self.devices = []

    def _get_build_id(self, bid):
        build = bid if bid else self.build_id
        if isinstance(build, BuildBean):
            return build.build_id if build.build_id else build.id
        return build

    def _get_app_url(self, url):
        app = url if url else self.app_url
        if isinstance(app, AppBean):
            return app.get_app_url()
        return app

    def _get_test_suite_url(self, url):
        suite = url if url else self.test_suite_url
        if isinstance(suite, TestSuiteBean):
            return suite.get_test_suite_url()
        return suite

    def _get_devices(self, ds: list[DeviceInfo]):
        device_list = ds
        if not ds or len(ds) == 0:
            device_list = self.devices

        return [device.name for device in device_list]

    def espresso_build(self, app_url: AppBean | str = None, test_suite_url: TestSuiteBean | str = None, devices: list[DeviceInfo] = [BrowserStackDevice.default_device]) -> BuildBean:
        data = {
            'app': self._get_app_url(app_url),
            'testSuite': self._get_test_suite_url(test_suite_url),
            'devices': self._get_devices(devices)
        }
        if TEST_PROJECT:
            data['project'] = TEST_PROJECT
        if DEVICE_LOG_ENABLE:
            data['deviceLogs'] = True

        return BuildBean(self.post(self._espresso_build_url, json_data=data))

    def get_build_state(self, build_id: BuildBean | str = None) -> BuildBean:
        return BuildBean(self.get(self._build_state_get_url, build_id=self._get_build_id(build_id)))

    def list_recent_builds(self) -> list[BuildBean]:
        res = self.get(self._list_recent_builds_url)
        return [BuildBean(build) for build in res.value]

    def get_last_build_task(self) -> BuildBean:
        tasks = self.list_recent_builds()
        if len(tasks) > 0:
            return tasks[0]
        return None

    @staticmethod
    def split_device_chunks(devices, n):
        """Yield successive n-sized chunks from lst."""
        for i in range(0, len(devices), n):
            yield devices[i:i + n]

    def build_device_test(self, app_url: AppBean | str = None, test_suite_url: TestSuiteBean | str = None,
                          devices: list[DeviceInfo] = BrowserStackDevice.default_device) -> list[BuildBean]:
        build_result = []

        for split_devices in BrowserStackBuild.split_device_chunks(devices, MAX_PARALLEL * 2):
            build_bean = self.espresso_build(
                app_url, test_suite_url, split_devices)
            build_state = None
            while True:
                build_state = self.get_build_state(build_bean)
                if build_state.status.is_complete():
                    break
                logging.info('The test task is being executed: %s',
                             build_state.status)
                time.sleep(BUILD_QUERY_INTERVAL)

            if build_state.status.is_success():
                logging.info('test app all pass')
            else:
                logging.info('test app exist error')
            build_result.append(build_state)

        return build_result

    def get_build_test(self, build_bean: BuildBean):
        build_state = None
        while True:
            build_state = self.get_build_state(build_bean)
            if build_state.status.is_complete():
                break
            logging.info('The test task is being executed: %s',
                         build_state.status)
            time.sleep(BUILD_QUERY_INTERVAL)
        if build_state.status.is_success():
            logging.info('test app all pass')
        else:
            logging.info('test app exist error')
        return build_state


class BrowserStackSession(BrowserStack):
    # GET
    _session_details_get_url = 'app-automate/espresso/v2/builds/{build_id}/sessions/{session_id}'
    # GET
    _juint_report_get_url = 'app-automate/espresso/v2/builds/{build_id}/sessions/{session_id}/report'
    # GET
    _code_coverage_get_url = 'app-automate/espresso/v2/builds/{build_id}/sessions/{session_id}/coverage'

    def __init__(self, auth: str, build_id=None, session_id=None) -> None:
        super().__init__(auth)
        self.build_id: str = build_id
        self.session_id: str = session_id

    def _get_session_id(self, sid):
        session = sid if sid else self.session_id
        if isinstance(session, SessionBean):
            return session.id
        return session

    def _get_build_id(self, bid):
        build = bid if bid else self.build_id
        if isinstance(build, BuildBean):
            return build.build_id if build.build_id else build.id
        return build

    def _get(self, url, build_id=None, session_id=None):
        return self.get(url, build_id=self._get_build_id(build_id), session_id=self._get_session_id(session_id))

    def get_session_details(self, build_id: BuildBean | str = None, session_id: SessionBean | str = None):
        res = self._get(self._session_details_get_url, build_id, session_id)
        return SessionBean(res)

    def get_juint_report(self, build_id: BuildBean | str = None, session_id: SessionBean | str = None) -> str:
        return self._get(self._juint_report_get_url, build_id, session_id)

    def get_code_coverage(self, build_id: BuildBean | str = None, session_id: SessionBean | str = None):
        return self._get(self._code_coverage_get_url, build_id, session_id)


def get_last_build_test_task(user: str) -> BuildBean:
    try:
        build = BrowserStackBuild(user)
        task = build.get_last_build_task()
        if not task:
            logging.error(
                'There is no build test task, please create it and try again')
            return 12
        return build.get_build_test(task)
    except requests.exceptions.RequestException as e:
        logging.error('get last recent build info error: %s', e)
        raise e


def execute_remove_command(args):
    if args.apk:
        app = BrowserStackApp(args.user)
        app.delete_recent_app()

    if args.suite:
        suite = BrowserStackTestSuite(args.user)
        suite.delete_recent_test_suite()

    return 0


def parse_test_devices(api: list[int], all_api: bool, names: list[str]) -> list[DeviceInfo]:
    result = []
    device = BrowserStackDevice()
    if names:
        devices = set()
        for name in names:
            info = device.find_device(DeviceInfo(name))
            if info:
                devices.add(info)
            else:
                logging.error(
                    'The specified device name was not found: %s', name)
        result = list(devices)
    elif all_api:
        api = [33, 32, 31, 30, 29, 28, 27, 26, 25, 25, 24, 23, 22, 21]
        result = device.random_devices(api)
    else:
        result = device.random_devices(api)
    return result


def parse_test_app(user: str, custom_id: str, apk: Path) -> AppBean:
    app = BrowserStackApp(user)
    if apk and apk.is_file():
        bean = app.update_app(apk, custom_id)
    elif custom_id:
        bean = app.find_custom_id_app(custom_id)
    else:
        bean = app.get_last_uplad_app()
    return bean


def parse_test_suite(user: str, custom_id: str, suite_path: Path) -> TestSuiteBean:
    suite = BrowserStackTestSuite(user)
    if suite_path and suite_path.is_file():
        bean = suite.upload_test_suite(suite_path, custom_id)
    elif custom_id:
        bean = suite.find_custom_id_test_suite(custom_id)
    else:
        bean = suite.get_last_test_suite()
    return bean


def execute_test_command(args):
    global MAX_PARALLEL
    MAX_PARALLEL = args.max_parallel
    global BUILD_QUERY_INTERVAL
    BUILD_QUERY_INTERVAL = args.query_interval

    if args.is_32bit:
        if args.apk_custom_id:
            args.apk_custom_id += '32'
        if args.project:
            args.project += '32'

    if args.project:
        global TEST_PROJECT
        TEST_PROJECT = args.project

    global DEVICE_LOG_ENABLE
    DEVICE_LOG_ENABLE = args.device_log

    if args.get_last_build:
        get_last_build_test_task(args.user)
        return 0

    if args.build_last_faild:
        task = get_last_build_test_task(args.user)
        test_devices = task.parse_failed_device()
    else:
        test_devices = parse_test_devices(args.api, args.all_api, args.devices)

    if not test_devices:
        logging.error('There is no device to test')
        return 10

    app_bean = parse_test_app(args.user, args.apk_custom_id, args.apk)
    if not app_bean:
        logging.error(
            'The test apk does not exist or the upload path `%s` does not exist, please upload and try again', args.apk)
        return 11

    suite_bean = parse_test_suite(
        args.user, args.test_suite_custom_id, args.test_suite)
    if not suite_bean:
        logging.error(
            'The test suite does not exist or the upload path `%s` does not exist, please upload and try again', args.test_suite)
        return 12

    try:
        build = BrowserStackBuild(args.user)
        success = True
        for state in build.build_device_test(app_bean, suite_bean, test_devices):
            if state.status.is_success():
                logging.info('test build %s all passed', state.id)
            else:
                logging.error('test build %s has errors', state.id)
                success = False
        return 0 if success else 13
    except requests.exceptions.RequestException as e:
        logging.error('test app error: %s', e)
        raise e


class PathAction(argparse._StoreAction):
    def __init__(self, option_strings, dest, must_exist=True, nargs=None, **kwargs) -> None:
        self._must_exist = must_exist
        super().__init__(option_strings, dest, nargs, **kwargs)

    def check_value(self, value, option_string):
        path = Path(value).resolve()
        if self._must_exist and not path.exists():
            name = option_string if option_string else self.dest.upper()
            raise argparse.ArgumentError(
                self, f'input path does not exist: {name} {path}')
        return path

    def __call__(self, parser, namespace, values, option_string=None) -> None:
        if values == None or len(values) < 1:
            raise argparse.ArgumentError(
                self, f'The required path parameter does not exist: {option_string}')
        if isinstance(values, list):
            paths = [self.check_value(x, option_string) for x in values]
        else:
            paths = self.check_value(values, option_string)
        setattr(namespace, self.dest, paths)


class FileAction(PathAction):
    def check_value(self, value, option_string):
        p = Path(value).resolve()
        if (p.exists() and not p.is_file()) or (self._must_exist and not p.is_file()):
            name = option_string if option_string else self.dest.upper()
            raise argparse.ArgumentError(
                self, f'Invalid file path input: {name} {p}')
        return p


class DirectoryAction(argparse._StoreAction):

    def check_value(self, value, option_string):
        p = Path(value).resolve()
        if (p.exists() and not p.is_dir()) or (self._must_exist and not p.is_dir()):
            name = option_string if option_string else self.dest.upper()
            raise argparse.ArgumentError(
                self, f'Invalid directory path input: {name} {p}')
        return p


class RequiredAction(argparse._StoreAction):

    def __init__(self, option_strings, dest, required_actions: list[argparse.Action] = [],  nargs='?', **kwargs) -> None:
        self._actions = required_actions
        super().__init__(option_strings, dest, nargs='?', const=True, **kwargs)

    def get_requires(self):
        return self._actions

    def __call__(self, parser, namespace, values, option_string=None) -> None:
        return super().__call__(parser, namespace, values, option_string)


class TrueRequiredAction(RequiredAction):
    def __call__(self, parser, namespace, values, option_string=None) -> None:
        for action in self.get_requires():
            action.required = True
        super().__call__(parser, namespace, values, option_string)


class FalseRequiredAction(RequiredAction):
    def __call__(self, parser, namespace, values, option_string=None):
        for action in self.get_requires():
            action.required = False
        super().__call__(parser, namespace, values, option_string)


def parse_argument():
    parser = argparse.ArgumentParser('fakelinker_browserstack_test', description='Automated testing of the fakelinker project',
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-i', '--log', help='Show more log',
                        action='store_true')
    subparser = parser.add_subparsers(description='test subcommand')

    remove_cmd = subparser.add_parser(
        'remove', help='Remove app/test suite/build etc.')
    remove_cmd.add_argument(
        '-u', '--user', help='Specify BrowserStack access key', required=True)
    remove_cmd.add_argument(
        '-a', '--apk', help='Remove test apk', action='store_true')
    remove_cmd.add_argument(
        '-s', '--suite', help='Remove test suite', action='store_true')
    remove_cmd.add_argument(
        '-b', '--build', help='Remove test build', action='store_true')
    remove_cmd.set_defaults(func=execute_remove_command)

    test_cmd = subparser.add_parser('test', help='Run test tasks')
    test_cmd.add_argument(
        '-u', '--user', help='Specify BrowserStack access key', required=True)
    test_cmd.add_argument('--apk', help='apk to be tested', action=FileAction)
    test_cmd.add_argument(
        '--is-32bit', help='Specifies that the test apk is a 32-bit program', action='store_true')
    test_cmd.add_argument('--apk-custom-id', help='Specify apk custom id',
                          type=str, default='FakelinkerTestApp')
    test_cmd.add_argument(
        '--test-suite', help='specify test suite', action=FileAction)
    test_cmd.add_argument('--test-suite-custom-id', help='Specify test suite custom id',
                          type=str, default='FakelinkerTestSuite')
    test_cmd.add_argument(
        '--api', help='Specify the api level of the test', type=int, nargs='+', default=33)
    test_cmd.add_argument(
        '--all-api', help='Execute one test per api level', action='store_true')
    test_cmd.add_argument(
        '--devices', help='Specify the name of the device to test', type=str, nargs='+')
    test_cmd.add_argument(
        '--max-parallel', help='The maximum number of parallel tests', type=int, default=5)
    test_cmd.add_argument('--query-interval',
                          help='Specifies the time interval for query build test tasks', type=int, default=15)

    test_cmd.add_argument(
        '--repeat-last', help='Test again with recent test apk and test suite', action='store_true')
    test_cmd.add_argument(
        '--get-last-build', help='Get recent build test information', action='store_true')
    test_cmd.add_argument('--build-last-faild',
                          help='Select the device that failed the upload test to test again', action='store_true')
    test_cmd.add_argument(
        '--project', help='Set test project name', type=str, default='fakelinker')
    test_cmd.add_argument(
        '--device-log', help='Open test task log', action='store_true')
    test_cmd.set_defaults(func=execute_test_command)
    args = parser.parse_args()
    args.print_help = parser.print_help
    if args.log:
        logging.basicConfig(level=logging.DEBUG)
    return args


def main():

    args = parse_argument()
    if hasattr(args, 'func'):
        sys.exit(args.func(args))
    else:
        args.print_help()
    sys.exit(1)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        pass
