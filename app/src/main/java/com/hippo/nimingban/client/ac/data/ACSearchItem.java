/*
 * Copyright 2015 Hippo Seven
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.hippo.nimingban.client.ac.data;

import android.os.Parcel;
import android.text.TextUtils;

import com.hippo.nimingban.client.ac.ACUrl;
import com.hippo.nimingban.client.data.Reply;
import com.hippo.nimingban.client.data.Site;
import com.hippo.text.Html;

public class ACSearchItem extends Reply {

    public String content;
    public String title;
    public String sage;
    public String email;
    public String now;
    public String userid;
    public String img;
    public String resto;
    public String ext;
    public String id;

    private Site mSite;

    private long mTime;
    private CharSequence mUser;
    private CharSequence mContent;
    private String mThumb;
    private String mImage;

    @Override
    public void generate(Site site) {
        mSite = site;

        mTime = ACPost.parseTime(now);

        mUser = Html.fromHtml(userid);

        mContent = ACPost.generateContent(content, sage, title, "", email);

        if (!TextUtils.isEmpty(img)) {
            String ext2 = ext;
            if (ext2 != null) {
                ext2 = ext2.substring(0, Math.min(ext2.length(), 4));
            }
            mThumb = ACUrl.HOST + "/Public/Upload/thumb/" + img + ext2;
            mImage = ACUrl.HOST + "/Public/Upload/image/" + img + ext2;
        }
    }

    @Override
    public Site getNMBSite() {
        return mSite;
    }

    @Override
    public String getNMBId() {
        return id;
    }

    @Override
    public String getNMBPostId() {
        return (TextUtils.isEmpty(resto) || resto.equals("0")) ? id : resto;
    }

    @Override
    public long getNMBTime() {
        return mTime;
    }

    @Override
    public CharSequence getNMBDisplayUsername() {
        return mUser;
    }

    @Override
    public CharSequence getNMBDisplayContent() {
        return mContent;
    }

    @Override
    public String getNMBThumbUrl() {
        return mThumb;
    }

    @Override
    public String getNMBImageUrl() {
        return mImage;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(this.content);
        dest.writeString(this.title);
        dest.writeString(this.sage);
        dest.writeString(this.email);
        dest.writeString(this.now);
        dest.writeString(this.userid);
        dest.writeString(this.img);
        dest.writeString(this.resto);
        dest.writeString(this.ext);
        dest.writeString(this.id);
        dest.writeInt(this.mSite.getId());
    }

    public ACSearchItem() {
    }

    protected ACSearchItem(Parcel in) {
        this.content = in.readString();
        this.title = in.readString();
        this.sage = in.readString();
        this.email = in.readString();
        this.now = in.readString();
        this.userid = in.readString();
        this.img = in.readString();
        this.resto = in.readString();
        this.ext = in.readString();
        this.id = in.readString();
        this.mSite = Site.fromId(in.readInt());
    }

    public static final Creator<ACSearchItem> CREATOR = new Creator<ACSearchItem>() {

        @Override
        public ACSearchItem createFromParcel(Parcel source) {
            return new ACSearchItem(source);
        }

        @Override
        public ACSearchItem[] newArray(int size) {
            return new ACSearchItem[size];
        }
    };
}
