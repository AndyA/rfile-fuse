/* rfile_fuse.c */

#define FUSE_USE_VERSION 26

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <fuse/fuse_opt.h>
#include <rfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct options {
  char *root;
} options;

struct open_file {
  rfile *rf;
  int fd;
};

#define RFILEFS_OPT_KEY(t, p, v) { t, offsetof(struct options, p), v }

enum {
  KEY_VERSION,
  KEY_HELP,
};

static struct fuse_opt rfile_opts[] = {
  RFILEFS_OPT_KEY("--root %s", root, 0),
  FUSE_OPT_KEY("-V",             KEY_VERSION),
  FUSE_OPT_KEY("--version",      KEY_VERSION),
  FUSE_OPT_KEY("-h",             KEY_HELP),
  FUSE_OPT_KEY("--help",         KEY_HELP),
  FUSE_OPT_END
};

static char *rfile_rel(const char *path) {
  size_t lroot = options.root ? strlen(options.root) : 0;
  size_t lpath = strlen(path);
  char *buf = malloc(lroot + lpath + 1);
  if (!buf) return NULL;
  if (options.root) memcpy(buf, options.root, lroot);
  memcpy(buf + lroot, path, lpath + 1);
  return buf;
}

static int rfile__getattr(const char *path, struct stat *stbuf) {
  char *rpath = rfile_rel(path);
  if (!rpath) return -ENOMEM;
  struct stat tmp;
  int rc = stat(rpath, stbuf);
  if (rc == 0 && S_ISREG(stbuf->st_mode) && !rfile_stat(rpath, &tmp))
    *stbuf = tmp;
  free(rpath);
  return rc ? -errno : 0;
}

static int rfile__readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi) {
  struct dirent ent, *ep;
  off_t pos = 0;
  int rc = 0;
  char *rpath = rfile_rel(path);
  if (!rpath) return -ENOMEM;

  (void) fi;

  DIR *d = opendir(rpath);
  if (!d) goto fail;

  for (pos = 0; pos < offset; pos++) {
    if (rc = readdir_r(d, &ent, &ep), rc) goto done;
    if (!ep) goto done;
  }

  for (;;) {
    if (rc = readdir_r(d, &ent, &ep), rc) break;
    if (!ep) break;
    if (filler(buf, ep->d_name, NULL, ++pos)) break;
  }

done:
  if (closedir(d)) return -errno;
fail:
  free(rpath);
  return rc ? -errno : 0;
}

static int rfile__open(const char *path, struct fuse_file_info *fi) {
  struct open_file *of = calloc(1, sizeof(struct open_file));
  if (NULL == of) return -ENOMEM;

  char *rpath = rfile_rel(path);
  if (!rpath) {
    free(of);
    return -ENOMEM;
  }

  if ((of->rf = rfile_open(rpath, fi->flags), !of->rf) &&
      (of->fd = open(rpath, fi->flags), of->fd < 0)) {
    free(rpath);
    free(of);
    return -errno;
  }

  fi->fh = (uint64_t) of;
  free(rpath);
  return 0;
}

static int rfile__read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
  (void) path;
  struct open_file *of = (struct open_file *) fi->fh;
  if (!of) return -EBADFD;

  ssize_t got;
  if (of->rf) {
    if (rfile_lseek(of->rf, offset, SEEK_SET) < 0) return -errno;
    got = rfile_read(of->rf, buf, size);
  }
  else {
    if (lseek(of->fd, offset, SEEK_SET) < 0) return -errno;
    got = read(of->fd, buf, size);
  }

  return (int) got;
}

static int rfile__release(const char *path, struct fuse_file_info *fi) {
  (void) path;
  struct open_file *of = (struct open_file *) fi->fh;
  if (of->rf) rfile_close(of->rf);
  if (of->fd) close(of->fd);
  free(of);
  fi->fh = 0;
  return 0;
}

static struct fuse_operations rfile_oper = {
  .getattr  = rfile__getattr,
  .readdir  = rfile__readdir,
  .open   = rfile__open,
  .read   = rfile__read,
  .release = rfile__release,
};

int main(int argc, char *argv[]) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  memset(&options, 0, sizeof(struct options));

  if (fuse_opt_parse(&args, &options, rfile_opts, NULL) == -1)
    return 1;

  ret = fuse_main(args.argc, args.argv, &rfile_oper, NULL);

  fuse_opt_free_args(&args);

  return ret;
}
