/*
 * //******************************************************************
 * //
 * // Copyright 2016 Samsung Electronics All Rights Reserved.
 * //
 * //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 * //
 * // Licensed under the Apache License, Version 2.0 (the "License");
 * // you may not use this file except in compliance with the License.
 * // You may obtain a copy of the License at
 * //
 * //      http://www.apache.org/licenses/LICENSE-2.0
 * //
 * // Unless required by applicable law or agreed to in writing, software
 * // distributed under the License is distributed on an "AS IS" BASIS,
 * // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * // See the License for the specific language governing permissions and
 * // limitations under the License.
 * //
 * //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 */
package org.iotivity.cloud.accountserver.resources.account;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.UUID;

import org.iotivity.cloud.accountserver.Constants;
import org.iotivity.cloud.accountserver.db.AccountDBManager;
import org.iotivity.cloud.accountserver.db.TokenTable;
import org.iotivity.cloud.accountserver.db.UserTable;
import org.iotivity.cloud.accountserver.oauth.OAuthProviderFactory;
import org.iotivity.cloud.accountserver.resources.acl.group.GroupResource;
import org.iotivity.cloud.accountserver.util.TypeCastingManager;
import org.iotivity.cloud.base.exception.ServerException.BadRequestException;
import org.iotivity.cloud.base.exception.ServerException.InternalServerErrorException;
import org.iotivity.cloud.base.exception.ServerException.NotFoundException;
import org.iotivity.cloud.base.exception.ServerException.UnAuthorizedException;
import org.iotivity.cloud.util.Log;

/**
 *
 * This class provides a set of APIs to handle requests about account
 * information of authorized user.
 *
 */
public class AccountManager {

    private OAuthProviderFactory           mFactory                  = null;
    private TypeCastingManager<UserTable>  mUserTableCastingManager  = new TypeCastingManager<>();
    private TypeCastingManager<TokenTable> mTokenTableCastingManager = new TypeCastingManager<>();

    public HashMap<String, Object> signUp(String did, String authCode,
            String authProvider, Object options) {

        boolean res = false;
        authProvider = checkAuthProviderName(authProvider);
        res = loadAuthProviderLibrary(authProvider);

        if (!res) {
            throw new InternalServerErrorException(
                    authProvider + " library is not loaded");
        }
        String userUuid = null;
        // set token data
        TokenTable tokenInfo = requestAccessToken(authCode, options);
        tokenInfo.setDid(did);
        tokenInfo.setProvider(authProvider);
        Date currentTime = new Date();
        DateFormat transFormat = new SimpleDateFormat("yyyyMMddkkmm");
        tokenInfo.setIssuedtime(transFormat.format(currentTime));

        // set user data
        UserTable userInfo = requestUserInfo(tokenInfo.getAccesstoken(),
                options);
        userInfo.setProvider(authProvider);

        // check uuid
        userUuid = findUuid(userInfo.getUserid(), authProvider);

        storeUserTokenInfo(userUuid, userInfo, tokenInfo, did);

        // make response
        HashMap<String, Object> response = makeSignUpResponse(tokenInfo);

        return response;
    }

    private void storeUserTokenInfo(String userUuid, UserTable userInfo,
            TokenTable tokenInfo, String did) {
        // store db
        if (userUuid == null) {
            userUuid = generateUuid();
            userInfo.setUuid(userUuid);

            AccountDBManager.getInstance().insertRecord(Constants.USER_TABLE,
                    castUserTableToMap(userInfo));

            // make my private group
            GroupResource.getInstance().createGroup(userInfo.getUuid(),
                    Constants.REQ_GTYPE_PRIVATE);
        }
        // add my device to private group
        GroupResource.getInstance().getGroup(userUuid)
                .addDevice(new HashSet<String>(Arrays.asList(did)));
        tokenInfo.setUuid(userUuid);
        AccountDBManager.getInstance().insertAndReplaceRecord(
                Constants.TOKEN_TABLE, castTokenTableToMap(tokenInfo));
    }

    private String checkAuthProviderName(String authProvider) {

        String authProviderName = null;

        if (authProvider.equalsIgnoreCase(Constants.GITHUB)) {
            authProviderName = Constants.GITHUB;
        } else if (authProvider.equalsIgnoreCase(Constants.SAMSUNG)) {
            authProviderName = Constants.SAMSUNG;
        }

        return authProviderName;
    }

    private String findUuid(String userId, String authProvider) {
        String uuid = null;

        HashMap<String, Object> condition = new HashMap<>();
        condition.put(Constants.KEYFIELD_USERID, userId);

        ArrayList<HashMap<String, Object>> recordList = AccountDBManager
                .getInstance().selectRecord(Constants.USER_TABLE, condition);

        for (HashMap<String, Object> record : recordList) {
            String foundProvider = record.get(Constants.KEYFIELD_PROVIDER)
                    .toString();
            if (foundProvider != null
                    && foundProvider.equalsIgnoreCase(authProvider)) {
                return record.get(Constants.KEYFIELD_UUID).toString();
            }
        }
        return uuid;
    }

    private HashMap<String, Object> castUserTableToMap(UserTable userInfo) {

        return mUserTableCastingManager.convertObjectToMap(userInfo);
    }

    private HashMap<String, Object> castTokenTableToMap(TokenTable tokenInfo) {

        return mTokenTableCastingManager.convertObjectToMap(tokenInfo);
    }

    private TokenTable castMapToTokenTable(HashMap<String, Object> record) {
        TokenTable tokenInfo = new TokenTable();
        return mTokenTableCastingManager.convertMaptoObject(record, tokenInfo);
    }

    private HashMap<String, Object> makeSignUpResponse(TokenTable tokenInfo) {

        HashMap<String, Object> response = new HashMap<>();

        response.put(Constants.RESP_ACCESS_TOKEN, tokenInfo.getAccesstoken());
        response.put(Constants.RESP_REFRESH_TOKEN, tokenInfo.getRefreshtoken());
        response.put(Constants.RESP_TOKEN_TYPE, Constants.TOKEN_TYPE_BEARER);
        response.put(Constants.RESP_EXPIRES_IN, tokenInfo.getExpiredtime());
        response.put(Constants.RESP_UUID, tokenInfo.getUuid());

        // It will be modified.
        response.put(Constants.RESP_REDIRECT_URI, getRegionCIUrl());
        response.put(Constants.RESP_CERTIFICATE, getRootCert());
        response.put(Constants.RESP_SERVER_ID, Constants.CLOUD_UUID);

        return response;
    }

    private String getRegionCIUrl() {

        // TODO: add region management
        return "coap+tcp://127.0.0.1:5683";
    }

    private byte[] getRootCert() {

        byte[] byteRootCert = null;

        Path path = Paths.get(Constants.ROOT_CERT_FILE);

        try {

            byteRootCert = Files.readAllBytes(path);

        } catch (IOException e) {

            e.printStackTrace();
            // throw new InternalServerErrorException(
            // "root cert file read failed!");
        }

        return byteRootCert;
    }

    private Boolean loadAuthProviderLibrary(String authProvider) {
        mFactory = new OAuthProviderFactory();

        return mFactory.load(authProvider);
    }

    private TokenTable requestAccessToken(String authCode, Object options) {
        TokenTable tokenInfo = mFactory.requestAccessTokenInfo(authCode,
                options);
        Log.d("access token : " + tokenInfo.getAccesstoken());
        Log.d("refresh token : " + tokenInfo.getRefreshtoken());
        Log.d("expired time" + tokenInfo.getExpiredtime());

        return tokenInfo;
    }

    private UserTable requestUserInfo(String accessToken, Object options) {
        UserTable userInfo = mFactory.requestGetUserInfo(accessToken, options);
        Log.d("user id  : " + userInfo.getUserid());

        return userInfo;
    }

    private String generateUuid() {
        UUID uuid = UUID.randomUUID();
        String userUuid = uuid.toString();
        Log.d("generated uuid : " + userUuid);
        return userUuid;
    }

    public HashMap<String, Object> signInOut(String uuid, String did,
            String accessToken) {

        // find record about uuid and did
        HashMap<String, Object> condition = new HashMap<>();
        condition.put(Constants.KEYFIELD_UUID, uuid);

        ArrayList<HashMap<String, Object>> recordList = findRecord(
                AccountDBManager.getInstance()
                        .selectRecord(Constants.TOKEN_TABLE, condition),
                Constants.KEYFIELD_DID, did);

        if (recordList.isEmpty()) {
            throw new UnAuthorizedException("access token doesn't exist");
        }

        HashMap<String, Object> record = recordList.get(0);

        TokenTable tokenInfo = castMapToTokenTable(record);

        if (verifyToken(tokenInfo, accessToken)) {
            long remainedSeconds = getRemainedSeconds(
                    tokenInfo.getExpiredtime(), tokenInfo.getIssuedtime());

            return makeSignInResponse(remainedSeconds);
        } else {
            throw new UnAuthorizedException("AccessToken is unauthorized");
        }
    }

    private ArrayList<HashMap<String, Object>> findRecord(
            ArrayList<HashMap<String, Object>> recordList, String fieldName,
            String value) {
        ArrayList<HashMap<String, Object>> foundRecord = new ArrayList<>();

        for (HashMap<String, Object> record : recordList) {
            Object obj = record.get(fieldName);
            if (obj != null && obj.equals(value)) {
                foundRecord.add(record);
            }
        }
        return foundRecord;
    }

    private HashMap<String, Object> makeSignInResponse(long remainedSeconds) {
        HashMap<String, Object> response = new HashMap<>();
        response.put(Constants.RESP_EXPIRES_IN, remainedSeconds);

        return response;
    }

    private long getRemainedSeconds(long expiredTime, String issuedTime) {
        if (expiredTime == Constants.TOKEN_INFINITE) {
            return Constants.TOKEN_INFINITE;
        } else {
            return expiredTime - getElaspedSeconds(issuedTime);
        }
    }

    private boolean verifyToken(TokenTable tokenInfo, String accessToken) {

        if (!checkAccessTokenInDB(tokenInfo, accessToken)) {
            return false;
        }

        if (tokenInfo.getExpiredtime() != Constants.TOKEN_INFINITE
                && !checkExpiredTime(tokenInfo)) {
            return false;
        }

        return true;
    }

    private boolean checkRefreshTokenInDB(TokenTable tokenInfo, String token) {
        if (tokenInfo.getRefreshtoken() == null) {
            Log.w("Refreshtoken doesn't exist");
            return false;
        } else if (!tokenInfo.getRefreshtoken().equals(token)) {
            Log.w("Refreshtoken is not correct");
            return false;
        }
        return true;
    }

    private boolean checkAccessTokenInDB(TokenTable tokenInfo, String token) {
        if (tokenInfo.getAccesstoken() == null) {
            Log.w("AccessToken doesn't exist");
            return false;
        } else if (!tokenInfo.getAccesstoken().equals(token)) {
            Log.w("AccessToken is not correct");
            return false;
        }
        return true;
    }

    private boolean checkExpiredTime(TokenTable tokenInfo) {

        String issuedTime = tokenInfo.getIssuedtime();
        long expiredTime = tokenInfo.getExpiredtime();

        long remainTime = getElaspedSeconds(issuedTime);

        if (remainTime > expiredTime) {
            Log.w("access token is expired");
            return false;
        }
        return true;
    }

    private long getElaspedSeconds(String issuedTime) {

        DateFormat format = new SimpleDateFormat("yyyyMMddkkmm");
        Date currentTime = new Date();
        Date issuedTimeDate = null;

        try {
            issuedTimeDate = format.parse(issuedTime);
        } catch (ParseException e) {
            e.printStackTrace();
        }

        long difference = currentTime.getTime() - issuedTimeDate.getTime();
        long elaspedSeconds = difference / 1000;
        Log.d("accessToken elasped time: " + elaspedSeconds + "s");

        return elaspedSeconds;
    }

    public HashMap<String, Object> refreshToken(String uuid, String did,
            String grantType, String refreshToken) {

        // find record about uuid and did
        HashMap<String, Object> condition = new HashMap<>();
        condition.put(Constants.KEYFIELD_UUID, uuid);

        ArrayList<HashMap<String, Object>> recordList = findRecord(
                AccountDBManager.getInstance()
                        .selectRecord(Constants.TOKEN_TABLE, condition),
                Constants.KEYFIELD_DID, did);

        if (recordList.isEmpty()) {
            throw new NotFoundException("refresh token doesn't exist");
        }

        HashMap<String, Object> record = recordList.get(0);

        TokenTable oldTokenInfo = castMapToTokenTable(record);

        if (!checkRefreshTokenInDB(oldTokenInfo, refreshToken)) {
            throw new NotFoundException("refresh token is not correct");
        }
        // call 3rd party refresh token method
        TokenTable newTokenInfo = requestRefreshToken(refreshToken);

        // record change
        oldTokenInfo.setAccesstoken(newTokenInfo.getAccesstoken());
        oldTokenInfo.setRefreshtoken(newTokenInfo.getRefreshtoken());

        // insert record
        AccountDBManager.getInstance().insertAndReplaceRecord(
                Constants.TOKEN_TABLE, castTokenTableToMap(oldTokenInfo));

        // make response
        HashMap<String, Object> response = makeRefreshTokenResponse(
                oldTokenInfo);

        return response;
    }

    private HashMap<String, Object> makeRefreshTokenResponse(
            TokenTable tokenInfo) {
        HashMap<String, Object> response = new HashMap<>();
        response.put(Constants.RESP_ACCESS_TOKEN, tokenInfo.getAccesstoken());
        response.put(Constants.RESP_REFRESH_TOKEN, tokenInfo.getRefreshtoken());
        response.put(Constants.RESP_TOKEN_TYPE, Constants.TOKEN_TYPE_BEARER);
        response.put(Constants.RESP_EXPIRES_IN, tokenInfo.getExpiredtime());

        return response;
    }

    private TokenTable requestRefreshToken(String refreshToken) {

        TokenTable tokenInfo = mFactory.requestRefreshTokenInfo(refreshToken);

        Log.d("access token : " + tokenInfo.getAccesstoken());
        Log.d("refresh token : " + tokenInfo.getRefreshtoken());
        Log.d("expired time : " + tokenInfo.getExpiredtime());

        return tokenInfo;
    }

    public HashMap<String, Object> searchUserAboutUuid(String uuid) {
        // search user info about uuid
        HashMap<String, Object> condition = new HashMap<>();
        condition.put(Constants.KEYFIELD_UUID, uuid);

        ArrayList<HashMap<String, Object>> recordList = AccountDBManager
                .getInstance().selectRecord(Constants.USER_TABLE, condition);
        HashMap<String, Object> response = makeSearchUserResponse(recordList);

        return response;
    }

    private HashMap<String, Object> makeSearchUserResponse(
            ArrayList<HashMap<String, Object>> recordList) {
        HashMap<String, Object> response = new HashMap<>();
        ArrayList<HashMap<String, Object>> ulist = new ArrayList<>();

        for (HashMap<String, Object> record : recordList) {
            HashMap<String, Object> uInfo = new HashMap<>();
            String uid = record.get(Constants.KEYFIELD_UUID).toString();
            uInfo.put(Constants.RESP_UUID, uid);
            record.remove(Constants.KEYFIELD_UUID);
            uInfo.put(Constants.RESP_USER_INFO, record);
            ulist.add(uInfo);
        }

        response.put(Constants.RESP_USER_LIST, ulist);
        Log.d("User List " + response.toString());

        return response;
    }

    // TODO: It will be changed
    public HashMap<String, Object> searchUserAboutCriteria(String criteria) {
        // parse criteria
        String[] searchType = getSearchType(criteria);

        // search user info about criteria
        HashMap<String, Object> condition = new HashMap<>();
        condition.put(searchType[0], searchType[1]);

        ArrayList<HashMap<String, Object>> recordList = AccountDBManager
                .getInstance().selectRecord(Constants.USER_TABLE, condition);
        HashMap<String, Object> response = makeSearchUserResponse(recordList);
        return response;
    }

    // TODO: It will be changed
    private String[] getSearchType(String criteria) {
        String[] searchType = criteria.split(":");
        String searchKey = searchType[0];
        String searchValue = searchType[1];

        if (searchKey == null || searchValue == null) {
            throw new BadRequestException("search key or value is null");
        }

        return searchType;
    }

    public void deleteDevice(String uid, String di) {

        HashSet<String> diSet = new HashSet<String>();
        diSet.add(di);

        // the group that gid is uid is my group.
        GroupResource.getInstance().removeGroupDevice(uid, diSet);
    }
}
