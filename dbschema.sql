-- Could have saved two table here. But having them avoids redundancy as well as
-- make it possible for future enhancements.

CREATE TABLE artist (
	name varchar(200) NOT NULL,
	PRIMARY KEY(name)
);

CREATE TABLE song (
	title varchar(200) NOT NULL,
	album varchar(200),
	artistname varchar(200),
	genrename varchar(200),
	filepath varchar(255),
	year int,
	track varchar(8),
	extension varchar(50),
	mtime int,
	PRIMARY KEY(title, artistname, album, year)
);

CREATE TABLE genre (
	name varchar(200) NOT NULL,
	PRIMARY KEY(name)
);

CREATE TABLE path (
	path varchar(255),
	active integer NOT NULL,
	PRIMARY KEY(path)
);
